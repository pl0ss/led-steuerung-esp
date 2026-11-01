import { computed, inject, Injectable, signal } from '@angular/core';
import { Preferences } from '@capacitor/preferences';
import { distinctUntilChanged, map } from 'rxjs';
import { BleService } from 'src/app/services/ble.service';

// 'solid' oder die id einer Animation, die der ESP32 meldet (Endpoint "animations")
export type LedEffect = string;

export interface LedAnimation {
  id: string;
  label: string;
}

// Wie oft der ESP32 nach seinem Zustand gefragt wird. Nötig, weil die Hardware Buttons am ESP32
// den Zustand ändern können, ohne dass die App davon etwas mitbekommt.
const STATE_POLL_INTERVAL_MS = 3000;

// Nach einem eigenen Kommando die Antwort des nächsten Polls ignorieren, sie könnte noch den
// Stand von vor dem Kommando enthalten und die UI kurz zurückspringen lassen.
const STATE_POLL_QUIET_MS = 1500;

// Antwort des ESP32 auf den Endpoint "state"
interface EspState {
  r: number;
  g: number;
  b: number;
  brightness: number;
  count: number;
  length: number;
  power: boolean;
  effect: string;
}

// Harte Obergrenze der Firmware, muss zu LED_STRIP_MAX_LEDS in led_strip_ctrl.h passen
export const LED_STRIP_MAX_LEDS = 300;

// Muss zu LED_STRIP_DEFAULT_LED_COUNT in led_strip_ctrl.h passen
export const LED_STRIP_DEFAULT_LED_COUNT = 100;

// Was der Color Picker anzeigt, solange keine einfarbige Farbe leuchtet (Regenbogen oder aus).
// Ein <input type="color"> löst "change" nur aus, wenn sich der Wert ändert. Würde der Picker
// nach dem Regenbogen weiter die alte Farbe anzeigen, könnte genau diese Farbe nicht mehr
// gewählt werden.
export const COLOR_PICKER_NEUTRAL = '#000000';

const STORAGE_KEY_PREFIX = 'led-state:';

interface StoredLedState {
  stripLength: number;
  ledCount: number;
  brightness: number;
  color: string; // '#rrggbb', zuletzt gesetzte einfarbige Farbe
  effect: LedEffect;
  powered: boolean;
}

/**
 * Hält den LED Zustand der App (Streifenlänge, Anzahl, Helligkeit, Farbe, Effekt, an/aus),
 * speichert ihn pro ESP (deviceId) lokal und schickt ihn nach jedem Verbindungsaufbau
 * mit einer einzigen "sync" Nachricht an den ESP32.
 */
@Injectable({ providedIn: 'root' })
export class LedStateService {
  private readonly bleService = inject(BleService);

  public readonly stripLength = signal(LED_STRIP_DEFAULT_LED_COUNT);
  public readonly ledCount = signal(LED_STRIP_DEFAULT_LED_COUNT);
  public readonly brightness = signal(100);
  public readonly color = signal(COLOR_PICKER_NEUTRAL);
  public readonly effect = signal<LedEffect>('solid');
  public readonly powered = signal(true);

  // Alle Animationen, die der ESP32 kann (so funktioniert jede App mit jedem ESP32)
  public readonly animations = signal<LedAnimation[]>([]);

  public readonly pickerColor = computed(() =>
    this.powered() && this.effect() === 'solid'
      ? this.color()
      : COLOR_PICKER_NEUTRAL,
  );

  private deviceId: string | null = null;
  private pollTimer: ReturnType<typeof setInterval> | null = null;
  private polling = false;
  private lastCommandAt = 0;

  constructor() {
    this.bleService.currentDevice$
      .pipe(
        map((device) => device?.device.deviceId ?? null),
        distinctUntilChanged(),
      )
      .subscribe((deviceId) => {
        if (deviceId) {
          void this.onConnected(deviceId);
        } else {
          this.stopPolling();
          this.deviceId = null;
          this.animations.set([]);
        }
      });
  }

  //* Aktionen der UI

  public async setColor(hex: string): Promise<void> {
    const wasOff = !this.powered();

    this.color.set(hex);
    this.effect.set('solid');
    this.powered.set(true);
    await this.persist();

    await this.send('set_color', this.hexToRgb(hex));
    // Erst nach der Farbe einschalten, sonst blitzt kurz die alte Farbe auf
    if (wasOff) {
      await this.send('set_power', { on: true });
    }
  }

  // Läuft auf dem ESP32 weiter, bis eine neue Farbe oder eine andere Animation
  // gesendet wird, auch wenn die BLE-Verbindung währenddessen abbricht.
  public async startAnimation(id: string): Promise<void> {
    const wasOff = !this.powered();

    this.effect.set(id);
    this.powered.set(true);
    await this.persist();

    await this.send('start_animation', { name: id });
    if (wasOff) {
      await this.send('set_power', { on: true });
    }
  }

  public async setPower(on: boolean): Promise<void> {
    this.powered.set(on);
    await this.persist();
    await this.send('set_power', { on });
  }

  public async setBrightness(percent: number): Promise<void> {
    this.brightness.set(percent);
    await this.persist();
    await this.send('set_brightness', { brightness: percent });
  }

  public async setLedCount(count: number): Promise<void> {
    this.ledCount.set(Math.min(count, this.stripLength()));
    await this.persist();
    await this.send('set_led_count', { count: this.ledCount() });
  }

  /**
   * Wie viele LEDs der Streifen wirklich hat. Ist gerade der ganze Streifen an,
   * leuchtet er danach weiter komplett, sonst wird die Anzahl höchstens gekürzt.
   */
  public async setStripLength(length: number): Promise<void> {
    const newLength = Math.min(Math.max(Math.round(length), 1), LED_STRIP_MAX_LEDS);
    const allWereLit = this.ledCount() >= this.stripLength();

    this.stripLength.set(newLength);
    this.ledCount.set(allWereLit ? newLength : Math.min(this.ledCount(), newLength));
    await this.persist();

    await this.send('sync', {
      length: this.stripLength(),
      count: this.ledCount(),
    });
  }

  //* Verbindungsaufbau

  /**
   * Läuft nach jedem Verbindungsaufbau: gespeicherten Zustand dieses ESPs laden
   * (oder Defaults nehmen) und per "sync" an den ESP32 schicken.
   */
  private async onConnected(deviceId: string): Promise<void> {
    this.deviceId = deviceId;

    const stored = await this.load(deviceId);
    if (this.deviceId !== deviceId) {
      return; // währenddessen getrennt oder anderes Gerät
    }

    if (stored) {
      this.stripLength.set(stored.stripLength);
      this.ledCount.set(stored.ledCount);
      this.brightness.set(stored.brightness);
      this.color.set(stored.color);
      this.effect.set(stored.effect);
      this.powered.set(stored.powered);
    } else {
      this.stripLength.set(LED_STRIP_DEFAULT_LED_COUNT);
      this.ledCount.set(LED_STRIP_DEFAULT_LED_COUNT);
      this.brightness.set(100);
      this.color.set(COLOR_PICKER_NEUTRAL);
      this.effect.set('solid');
      this.powered.set(true);
    }

    // Länge und Anzahl immer, den Rest nur von einem bekannten Gerät: ein neues Gerät
    // soll nicht auf einmal in einer Fantasiefarbe leuchten
    const payload: Record<string, unknown> = {
      length: this.stripLength(),
      count: this.ledCount(),
    };
    if (stored) {
      payload['brightness'] = this.brightness();
      payload['effect'] = this.effect();
      if (this.effect() === 'solid') {
        Object.assign(payload, this.hexToRgb(this.color()));
      }
      payload['power'] = this.powered();
    }

    await this.send('sync', payload);
    if (this.deviceId !== deviceId) return;

    await this.loadAnimations();
    if (this.deviceId !== deviceId) return;

    this.startPolling();
  }

  //* Zustand vom ESP32 holen

  // Die Liste der Animationen kommt vom ESP32, die App kennt keine eigenen
  private async loadAnimations(): Promise<void> {
    try {
      const response = await this.bleService.queryEndpoint<{
        list: LedAnimation[];
      }>('animations');
      this.animations.set(Array.isArray(response.list) ? response.list : []);
    } catch (error) {
      console.error('Animationen vom ESP laden fehlgeschlagen', error);
    }
  }

  private startPolling(): void {
    this.stopPolling();
    this.pollTimer = setInterval(() => void this.pollState(), STATE_POLL_INTERVAL_MS);
  }

  private stopPolling(): void {
    if (this.pollTimer !== null) {
      clearInterval(this.pollTimer);
      this.pollTimer = null;
    }
  }

  /**
   * Übernimmt den Zustand des ESP32 (z.B. nach einem Tastendruck am Gerät) in die App und
   * speichert ihn, damit beim nächsten Verbindungsaufbau nicht der alte Stand zurückgeschickt wird.
   */
  private async pollState(): Promise<void> {
    const deviceId = this.deviceId;
    if (!deviceId || this.polling) return;
    if (Date.now() - this.lastCommandAt < STATE_POLL_QUIET_MS) return;

    this.polling = true;
    try {
      const state = await this.bleService.queryEndpoint<EspState>('state');

      // Währenddessen getrennt oder ein eigenes Kommando abgeschickt: Antwort ist veraltet
      if (this.deviceId !== deviceId) return;
      if (Date.now() - this.lastCommandAt < STATE_POLL_QUIET_MS) return;

      this.adopt(state);
    } catch (error) {
      console.error('ESP Zustand abfragen fehlgeschlagen', error);
    } finally {
      this.polling = false;
    }
  }

  private adopt(state: EspState): void {
    const hex =
      '#' +
      [state.r, state.g, state.b]
        .map((channel) => this.clampInt(channel, 0, 255, 0).toString(16).padStart(2, '0'))
        .join('');
    const stripLength = this.clampInt(state.length, 1, LED_STRIP_MAX_LEDS, this.stripLength());
    const ledCount = this.clampInt(state.count, 0, stripLength, this.ledCount());
    const brightness = this.clampInt(state.brightness, 0, 100, this.brightness());
    const effect = this.isValidEffect(state.effect) ? state.effect : this.effect();
    const powered = typeof state.power === 'boolean' ? state.power : this.powered();

    const changed =
      hex !== this.color() ||
      stripLength !== this.stripLength() ||
      ledCount !== this.ledCount() ||
      brightness !== this.brightness() ||
      effect !== this.effect() ||
      powered !== this.powered();
    if (!changed) return;

    this.color.set(hex);
    this.stripLength.set(stripLength);
    this.ledCount.set(ledCount);
    this.brightness.set(brightness);
    this.effect.set(effect);
    this.powered.set(powered);
    void this.persist();
  }

  //* Speicher

  private async persist(): Promise<void> {
    if (!this.deviceId) return;

    const state: StoredLedState = {
      stripLength: this.stripLength(),
      ledCount: this.ledCount(),
      brightness: this.brightness(),
      color: this.color(),
      effect: this.effect(),
      powered: this.powered(),
    };

    try {
      await Preferences.set({
        key: STORAGE_KEY_PREFIX + this.deviceId,
        value: JSON.stringify(state),
      });
    } catch (error) {
      console.error('LED Zustand speichern fehlgeschlagen', error);
    }
  }

  private async load(deviceId: string): Promise<StoredLedState | null> {
    try {
      const { value } = await Preferences.get({
        key: STORAGE_KEY_PREFIX + deviceId,
      });
      if (!value) return null;

      const parsed = JSON.parse(value) as Partial<StoredLedState>;
      const stripLength = this.clampInt(
        parsed.stripLength,
        1,
        LED_STRIP_MAX_LEDS,
        LED_STRIP_DEFAULT_LED_COUNT,
      );

      return {
        stripLength,
        ledCount: this.clampInt(parsed.ledCount, 0, stripLength, stripLength),
        brightness: this.clampInt(parsed.brightness, 0, 100, 100),
        color: /^#[0-9a-f]{6}$/i.test(parsed.color ?? '')
          ? (parsed.color as string)
          : COLOR_PICKER_NEUTRAL,
        effect: this.isValidEffect(parsed.effect) ? parsed.effect : 'solid',
        powered: parsed.powered !== false,
      };
    } catch (error) {
      console.error('LED Zustand laden fehlgeschlagen', error);
      return null;
    }
  }

  //* Utils

  // Ein Fehler beim Senden (z.B. Verbindung weg) soll die UI nicht kaputt machen, der
  // Zustand ist gespeichert und wird beim nächsten Verbindungsaufbau per "sync" nachgereicht
  private async send(type: string, payload?: unknown): Promise<void> {
    this.lastCommandAt = Date.now();
    try {
      await this.bleService.sendCommand(type, payload);
    } catch (error) {
      console.error(`BLE Kommando "${type}" fehlgeschlagen`, error);
    }
  }

  // 'solid' oder eine Animations-id, wie sie der ESP32 vergibt (kurz, nur a-z, 0-9 und _)
  private isValidEffect(value: unknown): value is string {
    return typeof value === 'string' && /^[a-z0-9_]{1,32}$/.test(value);
  }

  private clampInt(
    value: unknown,
    min: number,
    max: number,
    fallback: number,
  ): number {
    if (typeof value !== 'number' || !Number.isFinite(value)) return fallback;
    return Math.min(Math.max(Math.round(value), min), max);
  }

  // '#rrggbb' in einzelne 0..255 Kanäle zerlegen, passend zum { r, g, b } Payload des ESP32
  private hexToRgb(hex: string): { r: number; g: number; b: number } {
    const value = hex.replace('#', '');
    return {
      r: parseInt(value.substring(0, 2), 16),
      g: parseInt(value.substring(2, 4), 16),
      b: parseInt(value.substring(4, 6), 16),
    };
  }
}

import { computed, inject, Injectable, signal } from '@angular/core';
import { Preferences } from '@capacitor/preferences';
import { distinctUntilChanged, map } from 'rxjs';
import { BleService } from 'src/app/services/ble.service';

export type LedEffect = 'solid' | 'rainbow';

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

  public readonly pickerColor = computed(() =>
    this.powered() && this.effect() === 'solid'
      ? this.color()
      : COLOR_PICKER_NEUTRAL,
  );

  private deviceId: string | null = null;

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
          this.deviceId = null;
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

  // Läuft auf dem ESP32 weiter, bis eine neue Farbe (oder später eine andere
  // Animation) gesendet wird, auch wenn die BLE-Verbindung währenddessen abbricht.
  public async startRainbow(): Promise<void> {
    const wasOff = !this.powered();

    this.effect.set('rainbow');
    this.powered.set(true);
    await this.persist();

    await this.send('start_rainbow');
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
    const newLength = Math.min(
      Math.max(Math.round(length), 1),
      LED_STRIP_MAX_LEDS,
    );
    const allWereLit = this.ledCount() >= this.stripLength();

    this.stripLength.set(newLength);
    this.ledCount.set(
      allWereLit ? newLength : Math.min(this.ledCount(), newLength),
    );
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
      if (this.effect() === 'rainbow') {
        payload['effect'] = 'rainbow';
      } else {
        payload['effect'] = 'solid';
        Object.assign(payload, this.hexToRgb(this.color()));
      }
      payload['power'] = this.powered();
    }

    await this.send('sync', payload);
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
        effect: parsed.effect === 'rainbow' ? 'rainbow' : 'solid',
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
    try {
      await this.bleService.sendCommand(type, payload);
    } catch (error) {
      console.error(`BLE Kommando "${type}" fehlgeschlagen`, error);
    }
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

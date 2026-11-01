import { Component, inject } from '@angular/core';
import { LedStateService } from 'src/app/services/led-state.service';
import { IonButton } from '@ionic/angular';

@Component({
  selector: 'app-color-picker',
  standalone: true,
  imports: [IonButton],
  template: `
    <div class="color-picker">
      <!-- Wert aus $event.target statt aus ngModel: kein Rennen zwischen "input" und "change".
           Der angezeigte Wert kommt aus dem LedStateService und fällt bei Regenbogen / aus auf
           neutral zurück, damit "change" auch für dieselbe Farbe nochmal feuert. -->
      <input
        type="color"
        [value]="ledState.pickerColor()"
        (change)="onColorChange($any($event.target).value)"
      />

      <!-- Nur die Animationen, die der ESP32 selbst meldet. Die aktive ist gefüllt. -->
      <div class="presets" style="margin-top: 10px;">
        @for (animation of ledState.animations(); track animation.id) {
          <ion-button
            [fill]="isActive(animation.id) ? 'solid' : 'outline'"
            (click)="startAnimation(animation.id)"
            >{{ animation.label }}</ion-button
          >
        }
      </div>
    </div>
  `,
})
export class ColorPickerComponent {
  protected readonly ledState = inject(LedStateService);

  constructor() {}

  setPreset(hex: string): void {
    void this.onColorChange(hex);
  }

  async onColorChange(hex: string): Promise<void> {
    await this.ledState.setColor(hex);
  }

  // Läuft auf dem ESP32 weiter, bis eine neue Farbe (oder später eine andere
  // Animation) gesendet wird, auch wenn die BLE-Verbindung währenddessen abbricht.
  async startAnimation(id: string): Promise<void> {
    await this.ledState.startAnimation(id);
  }

  isActive(id: string): boolean {
    return this.ledState.powered() && this.ledState.effect() === id;
  }

  // hexToRgb (und damit das Senden an den ESP32) liegt jetzt im LedStateService
}

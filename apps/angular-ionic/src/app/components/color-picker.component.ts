import { Component, inject } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { BleService } from 'src/app/services/ble.service';

@Component({
  selector: 'app-color-picker',
  standalone: true,
  imports: [FormsModule],
  template: `
    <div class="color-picker">
      <input
        type="color"
        [(ngModel)]="selectedColor"
        (change)="onColorChange()"
      />

      <!-- <div class="presets">
        <button (click)="setPreset('#ff0000')">Rot</button>
        <button (click)="setPreset('#00ff00')">Grün</button>
        <button (click)="setPreset('#0000ff')">Blau</button>
        <button (click)="setPreset('#ffffff')">Weiß</button>
      </div> -->
    </div>
  `,
})
export class ColorPickerComponent {
  public selectedColor = '#ffffff';

  private readonly bleService = inject(BleService);

  constructor() {}

  setPreset(hex: string): void {
    this.selectedColor = hex;
    void this.onColorChange();
  }

  async onColorChange(): Promise<void> {
    const { r, g, b } = this.hexToRgb(this.selectedColor);
    await this.bleService.sendCommand('set_color', { r, g, b });
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

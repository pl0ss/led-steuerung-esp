import { Component, inject, Input } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { BleService } from 'src/app/services/ble.service';
import { IonButton } from '@ionic/angular';

@Component({
  selector: 'app-led-slider',
  standalone: true,
  imports: [FormsModule, IonButton],
  template: `
    <div class="led-slider">
      <div class="led-slider__header">
        <span class="led-slider__label">{{ label }}</span>
        <span class="led-slider__value" style="margin-left: 5px;"
          >{{ value }}{{ unit }}</span
        >
      </div>

      <div style="display: flex; ">
        <ion-button (click)="minus()">-</ion-button>
        <input
          type="range"
          [min]="min"
          [max]="max"
          [step]="step"
          [(ngModel)]="value"
          (change)="onChange()"
          style="width: 100%; margin: 0px 10px"
        />
        <ion-button (click)="plus()">+</ion-button>
      </div>
    </div>
  `,
})
export class LedSliderComponent {
  @Input({ required: true }) label!: string;
  @Input({ required: true }) min!: number;
  @Input({ required: true }) max!: number;
  @Input({ required: true }) step!: number;
  @Input() unit = '';

  // BLE Command Type und Payload-Feldname, passend zum Protokoll auf dem ESP32
  @Input({ required: true }) commandType!: string;
  @Input({ required: true }) payloadKey!: string;

  @Input() value = 0;

  private readonly bleService = inject(BleService);

  constructor() {}

  async onChange(): Promise<void> {
    await this.bleService.sendCommand(this.commandType, {
      [this.payloadKey]: this.value,
    });
  }

  async minus() {
    this.value = Math.max(this.min, this.value - this.step);
    await this.onChange();
  }

  async plus() {
    this.value = Math.min(this.max, this.value + this.step);
    await this.onChange();
  }
}

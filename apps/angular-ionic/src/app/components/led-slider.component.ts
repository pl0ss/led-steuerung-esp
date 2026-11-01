import { Component, EventEmitter, Input, Output } from '@angular/core';
import { FormsModule } from '@angular/forms';
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

  // Schrittweite für - und +, falls sie größer sein soll als die des Sliders
  // (z.B. Slider auf 1 LED genau, die Buttons in 5er Schritten)
  @Input() buttonStep?: number;

  @Input() value = 0;

  // Das Senden an den ESP32 übernimmt der LedStateService, der Slider meldet nur den neuen Wert
  @Output() valueChange = new EventEmitter<number>();

  constructor() {}

  onChange(): void {
    this.valueChange.emit(this.value);
  }

  minus(): void {
    this.value = Math.max(
      this.min,
      this.value - (this.buttonStep ?? this.step),
    );
    this.onChange();
  }

  plus(): void {
    this.value = Math.min(
      this.max,
      this.value + (this.buttonStep ?? this.step),
    );
    this.onChange();
  }
}

import { Component, inject, OnInit, signal } from '@angular/core';
import {
  IonHeader,
  IonToolbar,
  IonTitle,
  IonContent,
  IonButton,
  IonInput,
} from '@ionic/angular';
import { BleService } from 'src/app/services/ble.service';
import {
  LED_STRIP_MAX_LEDS,
  LedStateService,
} from 'src/app/services/led-state.service';
import { ScanResult } from '@capacitor-community/bluetooth-le';
import { AsyncPipe } from '@angular/common';
import { ColorPickerComponent } from 'src/app/components/color-picker.component';
import { LedSliderComponent } from 'src/app/components/led-slider.component';

@Component({
  selector: 'app-control',
  templateUrl: 'control.page.html',
  styleUrls: ['control.page.scss'],
  imports: [
    IonHeader,
    IonToolbar,
    IonTitle,
    IonContent,
    IonButton,
    IonInput,
    AsyncPipe,
    ColorPickerComponent,
    LedSliderComponent,
  ],
})
export class ControlPage implements OnInit {
  public readonly bleService = inject(BleService);
  public readonly ledState = inject(LedStateService);

  public readonly maxStripLength = LED_STRIP_MAX_LEDS;

  public readonly autoConnect: boolean = true;

  public readonly bleDevices = signal<ScanResult[]>([]);
  public bleDevicesLoading: boolean = true;

  constructor() {}

  async ngOnInit(): Promise<void> {
    this.loadBleDevices();
  }

  public async loadBleDevices() {
    this.bleDevices.set([]);
    this.bleDevicesLoading = true;

    this.bleDevices.set(
      await this.bleService.getDevicesBleScan({ scanDurationMs: 3000 }),
    );
    this.bleDevicesLoading = false;

    if (this.autoConnect) {
      this.autoConnectToEsp();
    }
  }

  public async onStripLengthChange(event: CustomEvent): Promise<void> {
    const length = parseInt(String(event.detail.value), 10);
    if (Number.isNaN(length)) return;

    await this.ledState.setStripLength(length);

    // Das Feld zeigt sonst weiter z.B. "500" an, obwohl auf 300 begrenzt wurde
    (event.target as HTMLIonInputElement).value = this.ledState.stripLength();
  }

  /**
   * Nur automatisch verbinden, wenn genau ein ESP gefunden wurde, da sich die App anonsten zufällig mit dem erst besten verbinden würde
   */
  public autoConnectToEsp() {
    if (this.bleDevices().length == 1) {
      this.bleService.connectToBleDevice(this.bleDevices()[0]);
    }
  }
}

import { Component, inject, OnInit, signal } from '@angular/core';
import {
  IonHeader,
  IonToolbar,
  IonTitle,
  IonContent,
  IonButton,
} from '@ionic/angular';
import { BleService } from 'src/app/services/ble.service';
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
    AsyncPipe,
    ColorPickerComponent,
    LedSliderComponent,
  ],
})
export class ControlPage implements OnInit {
  public readonly bleService = inject(BleService);

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

    this.bleService.connectToBleDevice(this.bleDevices()[0]);
  }
}

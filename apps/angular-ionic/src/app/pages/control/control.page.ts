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

@Component({
  selector: 'app-control',
  templateUrl: 'control.page.html',
  styleUrls: ['control.page.scss'],
  imports: [IonHeader, IonToolbar, IonTitle, IonContent, IonButton, AsyncPipe],
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

  async testBleProtocol(): Promise<void> {
    try {
      // // 1. Gerät suchen und verbinden
      // const results = await this.bleService.getDevicesBleScan({
      //   scanDurationMs: 5000,
      // });
      // if (results.length === 0) {
      //   console.warn('Kein passendes Gerät gefunden');
      //   return;
      // }

      // await this.bleService.connectToBleDevice(results[0]);
      // console.log('Verbunden mit', results[0].device.deviceId);

      // 2. Command Kanal testen: sollte auf dem ESP im Log "ping received" auslösen
      await this.bleService.sendCommand('ping');
      console.log('ping gesendet');

      // 3. Query Kanal testen: sollte { uptime_ms, free_heap } zurückgeben
      const status =
        await this.bleService.queryEndpoint<StatusResponse>('status');
      console.log('status Endpoint:', status);

      // 4. Noch nicht implementierten Endpoint testen: sollte { implemented: false } zurückgeben
      const color = await this.bleService.queryEndpoint('color');
      console.log('color Endpoint:', color);
    } catch (error) {
      console.error('BLE Test fehlgeschlagen', error);
    } finally {
      // await this.bleService.disconnectFromBleDevice();
    }
  }
}

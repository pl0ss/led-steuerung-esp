import { Injectable } from '@angular/core';
import {
  BleClient,
  ScanMode,
  ScanResult,
} from '@capacitor-community/bluetooth-le';
import { BehaviorSubject, Observable } from 'rxjs';
import { BleUuid } from 'src/app/enums/ble-uuid.enum';

@Injectable({ providedIn: 'root' })
export class BleService {
  private readonly initPromise: Promise<void>; // Promise wird gespeichert, damit alle Methoden auf die Initialisierung warten können

  private readonly connectedToADeviceSubject = new BehaviorSubject<boolean>(
    false,
  );
  public readonly connectedToADevice$: Observable<boolean> =
    this.connectedToADeviceSubject.asObservable();

  private readonly currentDeviceSubject =
    new BehaviorSubject<ScanResult | null>(null);
  public readonly currentDevice$: Observable<ScanResult | null> =
    this.currentDeviceSubject.asObservable();

  constructor() {
    this.initPromise = this.initBle();
  }

  private async initBle(): Promise<void> {
    try {
      await BleClient.initialize();
    } catch (error) {
      console.error('BLE Initialisierung fehlgeschlagen', error);
      // Optional: Fehler weiterwerfen, damit initPromise rejected wird
      throw error;
    }
  }

  public async getDevicesBleScan({
    scanDurationMs = 5000,
  }: {
    scanDurationMs?: number;
  }): Promise<ScanResult[]> {
    // Sicherstellen, dass initialize() abgeschlossen ist, bevor gescannt wird
    await this.initPromise;

    const services: string[] = [BleUuid.Service];

    const results: ScanResult[] = [];

    await BleClient.requestLEScan(
      {
        allowDuplicates: false,
        scanMode: ScanMode.SCAN_MODE_LOW_LATENCY,
        services: services,
      },
      (res: ScanResult) => {
        results.push(res);
      },
    );

    // Scan läuft im Hintergrund weiter, deshalb hier eine feste Zeit warten
    await new Promise((resolve) => setTimeout(resolve, scanDurationMs));

    await BleClient.stopLEScan();

    return results;
  }

  public async connectToBleDevice(device: ScanResult) {
    try {
      await BleClient.connect(device.device.deviceId, () =>
        this.onDisconnect(device.device.deviceId),
      );

      this.currentDeviceSubject.next(device);
      this.connectedToADeviceSubject.next(true);
      console.log(`BLE connected: ${device.device.deviceId}`);
    } catch (error) {
      console.error('BLE connect fehlgeschlagen', error);
      throw error;
    }
  }

  private onDisconnect(deviceId: string) {
    this.currentDeviceSubject.next(null);
    this.connectedToADeviceSubject.next(false);
    console.log(`BLE disconnected: ${deviceId}`);
  }

  public async disconnectFromBleDevice() {
    const currentDevice = this.currentDeviceSubject.value;
    if (!currentDevice) return;

    await BleClient.disconnect(currentDevice.device.deviceId);
  }
}

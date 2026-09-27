import { Injectable } from '@angular/core';
import {
  BleClient,
  ScanMode,
  ScanResult,
  TimeoutOptions,
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

  /**
   * generische write Wrapper
   * @param deviceId
   * @param service
   * @param characteristic
   * @param value
   * @param options
   * @returns
   */
  public async write(
    deviceId: string,
    service: string,
    characteristic: string,
    value: DataView,
    options?: TimeoutOptions,
  ): Promise<void> {
    await this.initPromise;
    return BleClient.write(deviceId, service, characteristic, value, options);
  }

  /**
   * generische read Wrapper
   * @param deviceId
   * @param service
   * @param characteristic
   * @param options
   * @returns
   */
  public async read(
    deviceId: string,
    service: string,
    characteristic: string,
    options?: TimeoutOptions,
  ): Promise<DataView> {
    await this.initPromise;
    return BleClient.read(deviceId, service, characteristic, options);
  }

  /**
   * Prüft, ob device verbunden ist
   * @returns
   */
  private requireDeviceId(): string {
    const device = this.currentDeviceSubject.value;
    if (!device) {
      throw new Error('Kein BLE Gerät verbunden');
    }
    return device.device.deviceId;
  }

  private writeCommand(
    value: DataView,
    options?: TimeoutOptions,
  ): Promise<void> {
    return this.write(
      this.requireDeviceId(),
      BleUuid.CustomService,
      BleUuid.CommandCharacteristic,
      value,
      options,
    );
  }

  private writeQuery(value: DataView, options?: TimeoutOptions): Promise<void> {
    return this.write(
      this.requireDeviceId(),
      BleUuid.CustomService,
      BleUuid.QueryCharacteristic,
      value,
      options,
    );
  }

  private readQuery(options?: TimeoutOptions): Promise<DataView> {
    return this.read(
      this.requireDeviceId(),
      BleUuid.CustomService,
      BleUuid.QueryCharacteristic,
      options,
    );
  }

  //* JSON Protokoll Ebene, spiegelt gatt_svr / ble_write_handler / ble_read_handler

  /**
   * Sendet Kommando
   * @param type
   * @param payload
   * @returns
   */
  public sendCommand(type: string, payload?: unknown): Promise<void> {
    return this.writeCommand(this.encodeJson({ type, payload }));
  }

  /**
   * Wählt einen Endpoint aus und liest dessen JSON
   * @param endpoint
   * @param options
   * @returns
   */
  public async queryEndpoint<T = unknown>(
    endpoint: string,
    options?: TimeoutOptions,
  ): Promise<T> {
    await this.writeQuery(this.encodeJson({ endpoint }), options);
    const response = await this.readQuery(options);
    const parsed = this.decodeJson<{ endpoint: string; data: T }>(response);
    return parsed.data;
  }

  //* Utils
  private encodeJson(value: unknown): DataView {
    const json = JSON.stringify(value);
    return new DataView(new TextEncoder().encode(json).buffer);
  }

  private decodeJson<T>(value: DataView): T {
    const json = new TextDecoder().decode(value.buffer);
    return JSON.parse(json) as T;
  }
}

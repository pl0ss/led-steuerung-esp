import { Component, inject, OnInit, signal } from '@angular/core';
import {
  IonHeader,
  IonToolbar,
  IonTitle,
  IonContent,
  IonButton,
} from '@ionic/angular';
import { ExploreContainerComponent } from '../explore-container/explore-container.component';
import { BleService } from '../services/ble.service';
import { ScanResult } from '@capacitor-community/bluetooth-le';
import { AsyncPipe } from '@angular/common';

@Component({
  selector: 'app-tab1',
  templateUrl: 'tab1.page.html',
  styleUrls: ['tab1.page.scss'],
  imports: [IonHeader, IonToolbar, IonTitle, IonContent, IonButton, AsyncPipe],
})
export class Tab1Page implements OnInit {
  public readonly bleService = inject(BleService);

  public readonly bleDevices = signal<ScanResult[]>([]);
  public bleDevicesLoading: boolean = true;

  constructor() {}

  async ngOnInit(): Promise<void> {
    this.bleDevices.set(
      await this.bleService.getDevicesBleScan({ scanDurationMs: 3000 }),
    );
    this.bleDevicesLoading = false;
  }
}

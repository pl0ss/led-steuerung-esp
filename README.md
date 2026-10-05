# Struktur

repo/

- firmware/
- - esp32-common/ (Logic Bibliothek, reines C)
- - esp32-ble/ (Firmware Target mit BLE Trigger)
- - esp32-web-trigger/ (PC/Host Build mit HTTP Trigger)
- protocol/
- - ts/ (gemeinsames TS Package: Typen, Command Encoding/Decoding, UUIDs)
- - docs/ (GATT Service Beschreibung, Command Referenz)
- apps/
- - react-ionic/
- - next-ionic/
- - angular-ionic/

# Hardware

- ESP32 Doppelpack, einer reicht: 12€
  - https://www.amazon.de/Entwicklungsplatine-QIQIAZI-ESP32-WROOM-32-Bluetooth-Dual-Cores/dp/B0DHRV7784/
- LED Streifen: 5V WS2812B 1m 100 LEDs: 26€
  - https://www.ledzone.de/products/ws2812b-led-strip-1?variant=43528649801995

# First Installation

## ESP IDF Example Projekt auswählen

- VSC: ESP-IDF Extension

- Shift + CMD + P
- ESP-IDF: New Project
- ESP-IDF Examples -> bluetooth -> nimble -> blenroh

- Project Name: esp32-bleprph
- Enter Project directory: .../led-steuerung-esp/firmware

### idf.py ausführen

- VS Code mit ESP-IDF-Extension
- Strg+Shift+P und "ESP-IDF: Open ESP-IDF Terminal" ausführen

## angular-ionic App

- VSC: WebNative Extension
- Angular+lonic
- Targets: Web, iOS, Android
- Template: tabs

- ionic serve

- npm run build:ios
- npm run build:android
- npx cap open ios
- npx cap open android
- (siehe makefile)

## react-ionic App

- VSC: WebNative Extension
- React+lonic
- Targets: Web, iOS, Android
- Template: tabs

- ionic serve

- npm run build:ios
- npm run build:android
- npx cap open ios
- npx cap open android
- (siehe makefile)

### Icons

- https://ionic.io/ionicons

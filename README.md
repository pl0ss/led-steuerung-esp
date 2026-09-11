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

# First Installation

## ESP IDF Example Projekt auswählen

- VSC: ESP-IDF Extension

- Shift + CMD + P
- ESP-IDF: New Project
- ESP-IDF Examples -> bluetooth -> nimble -> blenroh

- Project Name: esp32-bleprph
- Enter Project directory: .../led-steuerung-esp/firmware

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

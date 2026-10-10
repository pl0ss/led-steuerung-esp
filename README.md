# LED Steuerung ESP 🌴✨🌈

Eine App, mit der ein LED-Streifen per Bluetooth gesteuert werden kann. Die App sendet Befehle per BLE (Bluetooth Low Energy) an einen ESP32, der den LED-Streifen ansteuert.

- Einzelne Farben setzen
- Ganze Animationen abspielen z. B. Regenbogen 🌈
- Betrieb per Powerbank möglich

**Motivation:** Der LED-Streifen soll in eine Laterne eingebaut werden, für eine Laternenwanderung an der Uni. 🏮

Du möchtest das Projekt nachbauen oder darauf aufbauen? Sehr gerne, siehe [Mitmachen](#mitmachen).

## Inhalt

- [Projektstruktur](#projektstruktur)
- [Hardware](#hardware)
- [Wichtige Befehle](#wichtige-befehle)
- [Einrichtung der Entwicklungsumgebung](#einrichtung-der-entwicklungsumgebung)
- [Verwendung der App](#verwendung-der-app)
- [Mitmachen](#mitmachen)
- [Erstinstallation (Doku zum Anlegen der Projekte)](#erstinstallation-doku-zum-anlegen-der-projekte)

<img height="400" alt="rainbow" src="https://github.com/user-attachments/assets/117b4cb0-a6bd-4e13-a4a9-6e05ddc3ce27" />
<img height="400" alt="setup" src="https://github.com/user-attachments/assets/15ebe152-6b52-4172-a67b-3fc5cc172503" />

## Projektstruktur

Die Struktur ist ein möglicher Entwurf und kann sich noch ändern.

```
repo/
├── firmware/
│   ├── esp32-common/        Logikbibliothek, reines C
│   ├── esp32-ble/           Firmware-Target mit BLE-Trigger
│   └── esp32-web-trigger/   PC/Host-Build mit HTTP-Trigger
├── protocol/
│   ├── ts/                  Gemeinsames TS-Package: Typen, Command Encoding/Decoding, UUIDs
│   └── docs/                GATT-Service-Beschreibung, Command-Referenz
├── apps/
│   ├── react-ionic/
│   ├── next-ionic/
│   └── angular-ionic/
├── makefile                 Kurzbefehle für die wichtigsten Aufgaben
└── README.md                Diese Datei
```

## Hardware

- ESP32
  - Doppelpack, einer reicht: [Amazon](https://www.amazon.de/Entwicklungsplatine-QIQIAZI-ESP32-WROOM-32-Bluetooth-Dual-Cores/dp/B0DHRV7784/) 12 €
- LED-Streifen
  - 5V WS2812B, 1 m, 100 LEDs: [LEDZone](https://www.ledzone.de/products/ws2812b-led-strip-1?variant=43528649801995) 26 €
- Stromversorgung: Netzteil oder Powerbank (5V)

## Wichtige Befehle

Zum Bauen und Starten der Apps gibt es Kurzbefehle im `makefile`:

| Befehl     | Beschreibung                                      |
| ---------- | ------------------------------------------------- |
| `make aa`  | Angular-Ionic als Web-App starten (`ionic serve`) |
| `make aai` | Angular-Ionic bauen und in Xcode öffnen           |
| `make aaa` | Angular-Ionic bauen und in Android Studio öffnen  |
| `make ar`  | React-Ionic als Web-App starten (`ionic serve`)   |
| `make ari` | React-Ionic bauen und in Xcode öffnen             |
| `make ara` | React-Ionic bauen und in Android Studio öffnen    |

## Einrichtung der Entwicklungsumgebung

### Voraussetzungen

- Node.js und npm
- Ionic CLI
- Xcode (für iOS) bzw. Android Studio (für Android)
- VS Code mit der Extension `ESP-IDF`

### Frontend

Es besteht die Möglichkeit, die App in verschiedenen Frameworks umzusetzen. Das Nötigste, um den LED-Streifen anzusteuern, ist bereits in `angular-ionic` umgesetzt.

#### Frontend: angular-ionic

1. Pakete installieren:

   ```bash
   cd apps/angular-ionic/ && npm install
   ```

2. Am Desktop als Web-App öffnen:

   ```bash
   ionic serve
   ```

3. Projekt bauen und in Xcode öffnen:

   ```bash
   make aai
   ```

4. Projekt bauen und in Android Studio öffnen:

   ```bash
   make aaa
   ```

### Firmware (ESP32)

Die Firmware wird mit dem ESP-IDF gebaut und geflasht.

1. VS Code Extension `ESP-IDF` installieren
2. ESP-IDF installieren (`v6.1.0`)

#### Builden und Flashen

In der unteren Leiste (BottomNav) von VS Code einstellen:

- ESP-IDF-Version: `v6.1.0`
- Flash-Methode: `UART`
- Port: passenden Port auswählen
- ESP-Modell: beim oben verlinkten ESP ist es `esp32`

Danach:

- Mit dem Schraubenschlüssel wird das Projekt gebaut.
- Mit der Flamme wird das Projekt gebaut, geflasht und im Monitor geöffnet.

Bei Bedarf kann der BLE-Gerätename im Code geändert werden:

```c
rc = ble_svc_gap_device_name_set("nimble-bleprph");
```

> Tipp: Wenn mehrere Geräte gleichzeitig im Einsatz sind (z. B. mehrere Laternen), vergib jedem Gerät einen eindeutigen Namen, damit sich die App mit dem richtigen ESP verbindet. Alternativ lassen sich die ESPs auch über ihre Device-ID unterscheiden, die in der App angezeigt wird.

#### ESP anschließen

Der GPIO-Pin für die Datenübertragung zum LED-Streifen ist `16`. Bei Bedarf kann er im Code geändert werden:

```c
#define LED_STRIP_GPIO_PIN 16
```

Der LED-Streifen braucht außerdem 5V und GND. Die Masse (GND) von ESP und LED-Streifen muss verbunden sein.

##### Verkabelung

ESP32 und LED-Streifen sind beide an die Masse des LED-Streifens angeschlossen. Im Bild sind das die zwei weißen Kabel und das schwarze Kabel. Rot ist Plus, Grün ist die Datenleitung. Zum Verbinden habe ich Lötverbinder verwendet, die ich mit einem Heißluftföhn erhitzt habe.

##### Reihenfolge beim Anschließen

1. Zuerst die Kabel am ESP32 anstecken: Weiß ist Masse, Grün ist die Datenleitung
2. Danach den Stecker des LED-Streifens an den LED-Streifen anschließen
3. Erst dann den ESP32 per USB-C-Kabel an eine Powerbank anschließen
4. Zum Schluss das USB-A-Kabel an die Powerbank anschließen, das den LED-Streifen mit Strom versorgt

<img height="400" alt="IMG_3787" src="https://github.com/user-attachments/assets/6c1b2a42-6638-45a2-98d0-859c16657ec0" />
<img height="400" alt="IMG_3785" src="https://github.com/user-attachments/assets/237faa91-f1c6-401a-afa0-bb63841812f1" />

#### `idf.py` ausführen, falls nötig

1. VS Code mit ESP-IDF-Extension öffnen
2. `Strg+Shift+P` (macOS: `Cmd+Shift+P`) und `ESP-IDF: Open ESP-IDF Terminal` ausführen

## Verwendung der App

- Bei mir funktionieren 100 LEDs bei 100 % Helligkeit sehr gut.
- Nicht bei jeder Einstellung werden die Farben bei mir richtig dargestellt.

## Mitmachen

Du hast Lust, an diesem Projekt mitzuwirken oder es selbst auszuprobieren? Gerne!

- Eigene Animationen ergänzen
- Weitere Frontends (React, Next) ausbauen
- Fehler melden oder Verbesserungen vorschlagen: Issues und Pull Requests sind willkommen :)

## Erstinstallation (Doku zum Anlegen der Projekte)

Mein Vorgehen beim Anlegen der Frameworks. **Das muss nicht ausgeführt werden!**

### ESP-IDF-Beispielprojekt auswählen

In VS Code mit der `ESP-IDF` Extension:

1. `Shift+Cmd+P` (Windows/Linux: `Strg+Shift+P`)
2. `ESP-IDF: New Project`
3. `ESP-IDF Examples` → `bluetooth` → `nimble` → `bleprph`
4. Project Name: `esp32-bleprph`
5. Project Directory: `.../led-steuerung-esp/firmware`

### Ionic-App anlegen (Angular oder React)

In VS Code mit der `WebNative` Extension:

1. Framework wählen: `Angular + Ionic` bzw. `React + Ionic`
2. Targets: `Web`, `iOS`, `Android`
3. Template: `tabs`

Danach:

```bash
ionic serve
npm run build:ios
npm run build:android
npx cap open ios
npx cap open android
```

Die wichtigsten Befehle stehen auch im `makefile`.

### Ionicons

Icons für die App gibt es hier: https://ionic.io/ionicons

# Tür-Display

Ein ESP32-S3 basiertes E-Paper-Display für die Tür — zeigt Informationen wie Status, Termine, Nachrichten oder beliebige Inhalte auf einem stromsparenden 200×200px E-Paper-Screen.

## Hardware

| Komponente | Details |
|---|---|
| MCU | Waveshare ESP32-S3 1.54inch E-Paper AIoT Development Board |
| Display | 200×200px E-Paper (1 bpp, MSB-first) |
| Buttons | BTN_REC (GPIO 0), BTN_PWR (GPIO 18) |
| Konnektivität | WLAN (802.11 b/g/n) |

## Dokumentation

| Dokument | Inhalt |
|---|---|
| [BEDIENUNGSANLEITUNG.md](BEDIENUNGSANLEITUNG.md) | Für Anwender: Bedienung, Anzeigen, Konfiguration |
| [CHANGELOG.md](CHANGELOG.md) | Für Entwickler: Änderungen, Begründungen, technische Details |

## Build & Flash

Voraussetzung: [arduino-cli](https://arduino.github.io/arduino-cli/) v1.5+, Core `esp32:esp32`

```bash
# Kompilieren
arduino-cli compile --fqbn esp32:esp32:esp32s3 \
  --build-property build.partitions=huge_app \
  "firmware 1.0/tuer_display"

# Flashen (Board in Bootloader-Modus: BOOT halten + RESET drücken)
arduino-cli upload --fqbn esp32:esp32:esp32s3 \
  --port /dev/cu.usbmodem2101 \
  "firmware 1.0/tuer_display"
```

## Versionen

| Version | Ordner | Beschreibung |
|---|---|---|
| v1.0 | `firmware 1.0` | WiFiManager, HA Tür/Fenster-Status, E-Paper Anzeige, Web-Config |

## Lizenz

Apache License 2.0 — siehe [LICENSE](LICENSE).  
Copyright 2025–2026 Andreas Liedke

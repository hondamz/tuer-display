# Tür-Display — Bedienungsanleitung

## Was ist das Tür-Display?

Das Tür-Display ist ein kleines, stromsparendes Gerät auf Basis des ESP32-S3-Mikrocontrollers mit einem 200×200 Pixel E-Paper-Bildschirm. Es ist dafür gedacht, an einer Tür oder Wand montiert zu werden und dort relevante Informationen anzuzeigen — z. B. Statusmeldungen, Termine, Nachrichten oder eigene Inhalte.

Das E-Paper-Display zeigt Informationen dauerhaft an, ohne Strom zu verbrauchen — das Bild bleibt auch bei ausgeschaltetem Gerät erhalten.

---

## Hardware-Übersicht

```
┌─────────────────────────┐
│     E-Paper Display     │  ← 200×200px, schwarz/weiß
│                         │
│                         │
│                         │
└────────┬────────┬───────┘
         │        │
      [BTN_REC] [BTN_PWR]
```

| Element | Funktion |
|---|---|
| E-Paper Display | Zeigt Informationen dauerhaft an |
| BTN_REC | Linke Taste (GPIO 0) |
| BTN_PWR | Rechte Taste (GPIO 18) |
| USB-C Anschluss | Stromversorgung und Firmware-Update |

---

## Inbetriebnahme

### 1. Stromversorgung

Das Gerät wird über den USB-C-Anschluss mit Strom versorgt. Nach dem Anschließen startet es automatisch.

### 2. WLAN-Konfiguration

*(Wird mit einer der nächsten Versionen beschrieben, sobald WLAN-Konfiguration implementiert ist.)*

---

## Tasten-Bedienung

*(Wird mit der jeweiligen Firmware-Version ergänzt.)*

---

## Anzeigen und Statusmeldungen

*(Wird mit der jeweiligen Firmware-Version ergänzt.)*

---

## Firmware aktualisieren

Eine Firmware-Aktualisierung erfordert eine USB-C-Verbindung zum Computer und wird vom Entwickler durchgeführt. Für Endanwender ist kein Firmware-Update notwendig.

---

## Technische Daten

| Parameter | Wert |
|---|---|
| Prozessor | ESP32-S3 (Dual-Core, 240 MHz) |
| RAM | 8 MB PSRAM |
| Flash | 8 MB |
| Display | 200×200px E-Paper, 1 bpp |
| Konnektivität | WLAN 802.11 b/g/n |
| Stromversorgung | USB-C, 5V |

---

*Dieses Dokument wird mit jeder neuen Firmware-Version aktualisiert.*

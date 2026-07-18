# Tür-Display — Entwickler-Changelog

Dieses Dokument beschreibt alle Firmware-Versionen mit den jeweiligen Änderungen, den Gründen dafür und technischen Hintergründen.

**Hardware:** Waveshare ESP32-S3 1.54inch E-Paper AIoT Development Board  
**Display:** 200×200px E-Paper (1 bpp, MSB-first, row-major)  
**Buttons:** BTN_REC (GPIO 0), BTN_PWR (GPIO 18)  
**FQBN:** `esp32:esp32:esp32s3`  
**Build-Property:** `build.partitions=huge_app`

---

## v1.0 — Initiales Projekt

**Ordner:** `firmware 1.0`  
**Datum:** 2026-07-18

### Projektstart

Neues Projekt auf Basis der bekannten Waveshare ESP32-S3 E-Paper-Hardware.  
Grundstruktur angelegt: Projektordner, Lizenz (Apache 2.0), README, Bedienungsanleitung und dieser Changelog.

**Warum Apache 2.0?**  
Im Gegensatz zur proprietären Lizenz des Diktat-AI-Projekts soll dieses Projekt offen und wiederverwendbar sein. Apache 2.0 erlaubt freie Nutzung, Modifikation und Weitergabe, auch in kommerziellen Projekten, solange die Lizenz- und Copyright-Hinweise erhalten bleiben.

### Hardware-Grundlagen (Referenz)

| Pin | Funktion |
|---|---|
| GPIO 0 | BTN_REC (Boot-Button) |
| GPIO 18 | BTN_PWR |
| GPIO 10 | EPD DC |
| GPIO 11 | EPD CS |
| GPIO 12 | EPD SCK |
| GPIO 13 | EPD MOSI |
| GPIO 9 | EPD RST |
| GPIO 8 | EPD BUSY |
| GPIO 6 | EPD PWR |

**E-Paper Bitmap-Format:**  
1 Bit pro Pixel, MSB-first, row-major. 1 = schwarz, 0 = weiß.  
Displaygröße: 200×200px = 5000 Bytes (200 × 200 / 8).

---

*Weitere Einträge folgen mit jeder Firmware-Erweiterung.*

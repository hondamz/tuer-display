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

## v1.0 — Tür- & Fensterstatus aus Home Assistant

**Ordner:** `firmware 1.0`  
**Datum:** 2026-07-18

### Neu: WiFiManager für WLAN-Konfiguration

Beim ersten Start (oder nach WLAN-Reset) öffnet das Gerät einen Access-Point **"TuerDisplay-Setup"**. Der Anwender verbindet sich per Smartphone und gibt im Browser (192.168.4.1) das WLAN-Passwort ein. Die Zugangsdaten werden dauerhaft im Flash gespeichert.

**Warum WiFiManager?**  
Keine hardcodierten WLAN-Zugangsdaten im Code — das Gerät ist ohne Neukompilierung in anderen Netzwerken nutzbar.

**Bibliothek:** `WiFiManager` by tzapu/tablatronix v2.0.17+

### Neu: Home Assistant Tür- & Fenster-Status

- Sensoren werden automatisch über **HA-Labels** per Auto-Discovery gefunden (`label_entities()` + `label_devices()` → Template-API)
- Abfrage des Sensor-Status in einem einzigen HTTP-Request (HA Template-Endpoint `/api/template`)
- Standard-Gruppen: **Fenster** (Label "Fenster") und **Tueren** (Label "Tuer")
- Label-Gruppen und HA-URL/Token sind über die Web-Konfig konfigurierbar

**Warum Template-Endpoint?**  
Ein Request für alle Sensoren statt N Einzelabfragen — deutlich effizienter (identischer Ansatz wie M5Stack-Dial-Projekt).

### Neu: E-Paper Statusanzeige

Monochrom 200×200px, Layout:
- **Header**: Anzahl offener Sensoren ("ALLES ZU" / "N OFFEN")
- **Sensoren-Liste**: Pro Gruppe ein Header mit Icon (Fenster/Tür/Allgemein), offene/gesamt Zähler, darunter jeder Sensor mit Name und Status (● offen / ○ zu)
- **Statuszeile**: WiFi OK/kein WiFi, "WEB" wenn Web-Portal aktiv, aktuelle Uhrzeit

**Warum Partial Refresh?**  
E-Paper Full Refresh (1–2 Sek.) würde bei jedem Poll sichtbar flackern. Partial Refresh ist wesentlich schneller (~300ms). Einmal pro Stunde wird ein Full Refresh erzwungen um Geister-Bilder (ghosting) zu verhindern.

### Neu: Tasten-Bedienung

| Taste | Funktion |
|---|---|
| BTN_REC (GPIO 0) | Manueller Refresh sofort |
| BTN_PWR (GPIO 18) | Web-Konfigurationsportal ein/aus |

### Neu: Web-Konfigurationsportal

Über BTN_PWR aktivierbar. Erreichbar unter `http://<geraete-ip>/`

| Seite | Inhalt |
|---|---|
| `/` | Geräteinformationen, manueller Refresh |
| `/ha` | HA URL, Access Token, Abfrageintervall |
| `/labels` | Label-Gruppen (Name, HA-Label, Symbol) |
| `/wifi` | WLAN-Status, WLAN-Reset |

### Neu: NTP Zeitsynchronisation

Automatische Zeitsynchronisation über NTP (pool.ntp.org) mit automatischer Sommer-/Winterzeitumstellung (Europe/Berlin, POSIX-Zeitzone `CET-1CEST,M3.5.0,M10.5.0/3`).

### Abhängigkeiten

| Bibliothek | Version | Zweck |
|---|---|---|
| WiFiManager by tzapu | 2.0.17+ | WLAN-Konfigurationsportal |
| Adafruit GFX Library | aktuell | E-Paper Text-Rendering |
| Adafruit BusIO | aktuell | Abhängigkeit von GFX |

### Technische Basis

- E-Paper Treiber und Draw-Primitives aus dem Diktat-AI Projekt (NoteAI) adaptiert
- HA Discovery-Logik aus dem M5Stack-Dial Projekt portiert
- Einstellungen persistent im NVS-Flash (Preferences, Namespace "tuerdisplay")

---

*Weitere Einträge folgen mit jeder Firmware-Erweiterung.*

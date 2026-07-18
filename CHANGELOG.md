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

## v1.1 — Einstellungsmenü, Web-Portal-Indikator, verbesserter WLAN-Boot

**Ordner:** `firmware 1.1`  
**Datum:** 2026-07-18

### Geändert: WLAN-Boot-Verhalten

Bisher lief der WiFiManager bei jedem Boot als Erstes und blockierte. Ab v1.1:

1. Beim Start wird versucht, mit den **gespeicherten WLAN-Zugangsdaten** zu verbinden (60 Sekunden Timeout).
2. Klappt das, läuft alles normal weiter — kein Portal, keine Interaktion nötig.
3. Schlägt die Verbindung fehl, erscheint auf dem Display:
   - Der Name des gespeicherten Netzes
   - `REC: 60s nochmal` → erneuter Verbindungsversuch
   - `PWR: WLAN einrichten` → startet WiFiManager-Portal
4. Gibt es noch keine gespeicherten Zugangsdaten (Ersteinrichtung), startet der WiFiManager-Portal direkt.

**Warum?** WLAN-Zugangsdaten bleiben jetzt dauerhaft gespeichert und werden nach einem Stromausfall automatisch verwendet, ohne dass der Anwender eingreifen muss.

**Technisch:** `WiFi.begin()` ohne Argumente verwendet die zuletzt im ESP32-Flash gespeicherten Zugangsdaten (werden vom WiFiManager beim ersten Setup einmalig geschrieben).

### Neu: Einstellungsmenü auf dem E-Paper-Display

Über einen **langen Druck auf BTN_PWR (≥ 500ms)** öffnet sich ein Einstellungsmenü:

- Zeigt aktuelles WLAN-Netz, IP-Adresse und Signalstärke
- **REC**: Web-Konfigurationsportal ein- oder ausschalten
- **PWR**: Zurück zur Hauptanzeige

Beim Verlassen wird ein Full-Refresh durchgeführt, um den Partial-Refresh-Modus korrekt wiederherzustellen.

### Neu: Web-Portal-Indikator im Display-Header

Wenn das Web-Konfigurationsportal aktiv ist, erscheint oben rechts im schwarzen Header-Balken ein kleines weißes **„W"**-Symbol. So ist der Portal-Status auf einen Blick erkennbar, ohne in die Statuszeile schauen zu müssen.

### Neu: Sensor-Anzeigenamen konfigurierbar (Web-Portal `/sensors`)

Im Web-Portal unter `/sensors` werden alle entdeckten Sensoren aufgelistet. Für jeden Sensor kann ein eigener Anzeigename eingegeben werden, der auf dem Display statt dem HA-Friendly-Name erscheint.

- Leeres Feld = HA-Name wird verwendet
- Namen werden im NVS gespeichert und bleiben nach einem Neustart erhalten
- Nach Discovery wird `applyNameOverrides()` aufgerufen, um die Überschreibungen sofort anzuwenden

### Geändert: Tasten-Belegung

| Taste | Kurz (<500ms) | Lang (≥500ms) |
|---|---|---|
| BTN_REC | Manueller Refresh | — |
| BTN_PWR | Web-Konfigurationsportal ein/aus | Einstellungsmenü öffnen |

### Geändert: Web-Portal-Navigation

Die Navigationsleiste enthält jetzt einen zusätzlichen Punkt:
`Info | HA | Labels | Sensoren | WLAN`

### Technische Details

- Neue Funktion `connectWifi()` kapselt die gesamte Boot-WiFi-Logik
- Neue Funktion `enterSettings()` ist blockierend und bedient das E-Paper-Einstellungsmenü
- Neue Funktion `reInitPartialAndShow()` stellt nach Full-Refresh den Partial-Modus wieder her
- `applyNameOverrides()` in `ha.cpp` wendet alle NVS-gespeicherten Namensüberschreibungen an
- NVS-Key `"nmOv"`: serialisiertes Format `"entityId|name~entityId|name~..."`

---

*Weitere Einträge folgen mit jeder Firmware-Erweiterung.*

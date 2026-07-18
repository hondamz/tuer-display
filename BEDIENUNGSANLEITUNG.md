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

### 2. WLAN-Konfiguration (Ersteinrichtung)

Beim ersten Start erscheint auf dem Display:

```
┌────────────────────────┐
│     TÜR-DISPLAY        │
│  WLAN einrichten:      │
│  1. Verbinde mit:      │
│  TuerDisplay-Setup     │
│  2. Oeffne Browser:    │
│  192.168.4.1           │
└────────────────────────┘
```

**Schritt für Schritt:**

1. Mit dem Smartphone oder Computer mit dem WLAN **"TuerDisplay-Setup"** verbinden
2. Browser öffnen und **192.168.4.1** aufrufen
3. Auf **"Configure WiFi"** klicken
4. Das Heimnetzwerk auswählen und das Passwort eingeben
5. Auf **"Save"** klicken — das Gerät verbindet sich und startet automatisch

Die WLAN-Zugangsdaten werden dauerhaft gespeichert und bei jedem Neustart automatisch verwendet.

### 3. Home Assistant Konfiguration

Damit das Gerät die Sensor-Daten abrufen kann, muss einmalig die Home-Assistant-Verbindung eingerichtet werden:

1. **BTN_PWR** drücken → Display zeigt "WEB" in der Statuszeile
2. Im Browser `http://<IP-Adresse>/ha` aufrufen (IP steht in der Statuszeile)
3. **URL** und **Long-Lived Access Token** eingeben (Token erstellen in HA unter: Profil → Sicherheit → Langlebige Zugriffstoken)
4. Speichern

### 4. Sensoren konfigurieren (HA-Labels)

Das Gerät findet Sensoren automatisch über HA-Labels. Voraussetzung: In Home Assistant müssen die gewünschten Tür-/Fensterkontakte mit den Labels **"Fenster"** und **"Tuer"** gekennzeichnet sein.

Labels in Home Assistant setzen:
- Einstellungen → Geräte & Dienste → Gerät öffnen → bei der Entität auf "Labels" klicken

---

## Tasten-Bedienung

| Taste | Position | Funktion |
|---|---|---|
| **BTN_REC** | Links (GPIO 0) | Sofortiger manueller Refresh |
| **BTN_PWR** | Rechts (GPIO 18) | Web-Konfigurationsportal ein/aus |

---

## Anzeigen und Statusmeldungen

### Hauptanzeige

```
┌────────────────────────┐
│  ████ 2 OFFEN ████     │  ← Anzahl offener Sensoren
├────────────────────────┤
│ □ Fenster       2/5    │  ← Gruppe: 2 von 5 offen
│  ● Wohnzimmer  OFFEN   │  ← ● = offen (Problem)
│  ○ Kueche      zu      │  ← ○ = geschlossen (OK)
│  ○ Bad         zu      │
├────────────────────────┤
│ □ Tueren        1/3    │
│  ● Haustuer    OFFEN   │
│  ○ Garage      zu      │
├────────────────────────┤
│ WiFi OK      │  14:35  │  ← Statuszeile
└────────────────────────┘
```

### Header-Zeile (oben)

| Anzeige | Bedeutung |
|---|---|
| `ALLES ZU` | Alle Sensoren geschlossen — alles OK |
| `1 OFFEN` | Genau ein Sensor ist offen |
| `N OFFEN` | N Sensoren sind offen |
| `TUER-DISPLAY` | Daten werden noch geladen |

### Sensor-Status

| Symbol | Bedeutung |
|---|---|
| ● (gefüllter Kreis) | Sensor offen — Handlungsbedarf |
| ○ (offener Kreis) | Sensor geschlossen — OK |

### Statuszeile (unten)

| Anzeige | Bedeutung |
|---|---|
| `WiFi OK` | Verbunden mit dem WLAN |
| `kein WiFi` | Keine WLAN-Verbindung |
| `WEB` | Web-Konfigurationsportal ist aktiv |
| `HH:MM` | Aktuelle Uhrzeit |

---

## Web-Konfigurationsportal

Nach dem Drücken von **BTN_PWR** ist das Konfigurationsportal erreichbar:

| Adresse | Inhalt |
|---|---|
| `http://<IP>/` | Geräteinformationen, Status, manueller Refresh |
| `http://<IP>/ha` | Home Assistant URL und Access Token |
| `http://<IP>/labels` | Sensor-Gruppen konfigurieren |
| `http://<IP>/wifi` | WLAN-Status, WLAN neu einrichten |

Die aktuelle IP-Adresse wird in der Statuszeile des Displays angezeigt, sobald WiFi verbunden ist.

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

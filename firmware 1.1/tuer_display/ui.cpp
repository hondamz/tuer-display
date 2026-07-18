#include "ui.h"
#include "draw.h"
#include "ha.h"
#include "config.h"

// ── Boot / status screens ─────────────────────────────────────────────────────

void showBoot(const char* msg) {
  clearWhite();
  fillRect(0, 0, EPD_W, 26, BLACK);
  drawStrC(EPD_W/2, 7, "TUER-DISPLAY", 1, WHITE);
  drawStrC(EPD_W/2, 100, msg, 1, BLACK);
  refreshFull();
}

void showWifiPortal(const char* apName, const char* ip) {
  clearWhite();
  fillRect(0, 0, EPD_W, 26, BLACK);
  drawStrC(EPD_W/2, 7, "TUER-DISPLAY", 1, WHITE);
  drawStrC(EPD_W/2, 50,  "WLAN einrichten:", 1, BLACK);
  drawStrC(EPD_W/2, 68,  "1. Verbinde mit:", 1, BLACK);
  drawStrC(EPD_W/2, 84,  apName,             1, BLACK);
  drawStrC(EPD_W/2, 108, "2. Oeffne Browser:", 1, BLACK);
  drawStrC(EPD_W/2, 124, ip,                  1, BLACK);
  refreshFull();
}

void showConnecting(const char* ssid) {
  clearWhite();
  fillRect(0, 0, EPD_W, 26, BLACK);
  drawStrC(EPD_W/2, 7, "TUER-DISPLAY", 1, WHITE);
  drawStrC(EPD_W/2, 80, "Verbinde mit:", 1, BLACK);
  drawStrFit(4, 98, EPD_W-8, ssid, 1, BLACK);
  refreshFull();
}

void showError(const char* msg) {
  clearWhite();
  fillRect(0, 0, EPD_W, 26, BLACK);
  drawStrC(EPD_W/2, 7, "FEHLER", 1, WHITE);
  drawStrFit(4, 60, EPD_W-8, msg, 1, BLACK);
  refreshFull();
}

void showWifiFail(const char* savedSsid) {
  clearWhite();
  fillRect(0, 0, EPD_W, 26, BLACK);
  drawStrC(EPD_W/2, 7, "TUER-DISPLAY", 1, WHITE);
  drawStrC(EPD_W/2, 50, "Keine WLAN-Verbindung", 1, BLACK);
  if (savedSsid && savedSsid[0]) {
    drawStrC(EPD_W/2, 70, "Gespeichertes Netz:", 1, BLACK);
    drawStrFit(4, 88, EPD_W-8, savedSsid, 1, BLACK);
  }
  hline(4, 110, EPD_W-8, BLACK);
  drawStrC(EPD_W/2, 128, "REC: 60s nochmal", 1, BLACK);
  drawStrC(EPD_W/2, 148, "PWR: WLAN einrichten", 1, BLACK);
  refreshFull();
}

void showSettings(const char* ssid, const char* ip, int rssi, bool webActive) {
  clearWhite();
  fillRect(0, 0, EPD_W, 26, BLACK);
  drawStrC(EPD_W/2, 7, "EINSTELLUNGEN", 1, WHITE);

  int y = 36;
  char buf[48];

  drawStr(4, y, "WLAN:", 1, BLACK);
  drawStrFit(52, y, EPD_W-56, ssid ? ssid : "---", 1, BLACK);
  y += 18;

  snprintf(buf, sizeof(buf), "IP: %s", ip ? ip : "---");
  drawStr(4, y, buf, 1, BLACK);
  y += 18;

  snprintf(buf, sizeof(buf), "Signal: %d dBm", rssi);
  drawStr(4, y, buf, 1, BLACK);
  y += 22;

  hline(4, y, EPD_W-8, BLACK);
  y += 12;

  drawStr(4, y, "Web-Konfig:", 1, BLACK);
  const char* stateStr = webActive ? "EIN" : "AUS";
  int tw = textW(stateStr, 1);
  if (webActive) {
    fillRect(EPD_W-4-tw-6, y-1, tw+12, 15, BLACK);
    drawStr(EPD_W-4-tw-3, y, stateStr, 1, WHITE);
  } else {
    strokeRect(EPD_W-4-tw-6, y-1, tw+12, 15, 1, BLACK);
    drawStr(EPD_W-4-tw-3, y, stateStr, 1, BLACK);
  }
  y += 28;

  hline(4, y, EPD_W-8, BLACK);
  y += 12;

  drawStrC(EPD_W/2, y, "REC: Web-Konfig an/aus", 1, BLACK);
  y += 18;
  drawStrC(EPD_W/2, y, "PWR: Zurueck", 1, BLACK);

  refreshFull();
}

// ── Icons ─────────────────────────────────────────────────────────────────────

// Window icon (small square with cross-bar – 12×12)
static void iconWindow(int x, int y, uint8_t c) {
  strokeRect(x, y, 12, 12, 1, c);
  hline(x, y+6, 12, c);
  vline(x+6, y, 6, c);
}

// Door icon (rectangle with small circle handle – 10×14)
static void iconDoor(int x, int y, uint8_t c) {
  strokeRect(x, y, 10, 14, 1, c);
  fillCircle(x+7, y+7, 1, c);
}

// Generic square (small open square – 10×10)
static void iconGeneric(int x, int y, uint8_t c) {
  strokeRect(x+1, y+2, 10, 10, 1, c);
}

// Circle: open = filled circle (warning), closed = outline circle (ok)
static void dotStatus(int cx, int cy, bool open) {
  if (open) fillCircle(cx, cy, 4, BLACK);
  else      strokeCircle(cx, cy, 4, 1, BLACK);
}

// ── Main door/window screen ───────────────────────────────────────────────────
//
// Layout (200×200):
//  y=0..25    Header bar (black)
//  y=26..170  Sensor list (scrolls through groups)
//  y=171..199 Status bar
//
// Per group:
//   Group header row: icon + name + "N offen"
//   Per sensor row:   dot + name + "OFFEN"/"ZU"
//
// With up to 6 groups × several sensors the list can be long.
// Current: display all groups and truncate at y=170.

void showDoorWindowScreen(const String& timeStr, const String& ipStr, bool wifiOk, bool webActive) {
  clearWhite();

  // ── Header ─────────────────────────────────────────────────────────────────
  fillRect(0, 0, EPD_W, 26, BLACK);
  int totalOpen = countOpen();
  char hdr[32];
  if (!dwDataValid) {
    snprintf(hdr, sizeof(hdr), "TUER-DISPLAY");
  } else if (totalOpen == 0) {
    snprintf(hdr, sizeof(hdr), "ALLES ZU");
  } else if (totalOpen == 1) {
    snprintf(hdr, sizeof(hdr), "1 OFFEN");
  } else {
    snprintf(hdr, sizeof(hdr), "%d OFFEN", totalOpen);
  }
  drawStrC(EPD_W/2, 7, hdr, 1, WHITE);

  // Web-Portal-Indikator oben rechts im Header (kleines "W" in weissem Kaestchen)
  if (webActive) {
    fillRect(EPD_W-18, 3, 14, 14, WHITE);
    drawStrC(EPD_W-11, 5, "W", 1, BLACK);
  }

  // ── Sensor list ────────────────────────────────────────────────────────────
  const int ROW_GROUP  = 19;
  const int ROW_SENSOR = 16;
  const int MARGIN_X   = 4;
  const int MAX_Y      = 143;  // Platz für IP-Zeile über dem Trennstrich lassen
  int y = 30;

  if (!dwDataValid) {
    drawStrC(EPD_W/2, 100, "Lade Sensoren...", 1, BLACK);
  } else if (discEntityCount == 0) {
    drawStrC(EPD_W/2, 80,  "Keine Sensoren", 1, BLACK);
    drawStrC(EPD_W/2, 96,  "konfiguriert.", 1, BLACK);
    drawStrC(EPD_W/2, 116, "Web-Config: BTN_PWR", 1, BLACK);
  } else {
    for (int g = 0; g < MAX_LABEL_GROUPS && y < MAX_Y; g++) {
      if (!labelGroups[g].used || !labelGroups[g].enabled) continue;
      // Check if this group has any discovered sensors
      if (countTotalInGroup(g) == 0) continue;

      // ── Group header row ──────────────────────────────────────────────────
      // Draw separator line before group (skip first)
      if (y > 30) { hline(MARGIN_X, y-2, EPD_W-2*MARGIN_X, BLACK); }

      // Icon
      if (labelGroups[g].iconType == LG_ICON_WINDOW)      iconWindow(MARGIN_X, y+2, BLACK);
      else if (labelGroups[g].iconType == LG_ICON_DOOR)   iconDoor(MARGIN_X, y+2, BLACK);
      else                                                  iconGeneric(MARGIN_X, y+2, BLACK);

      // Group name
      char gname[20];
      strlcpy(gname, labelGroups[g].displayName, sizeof(gname));
      drawStr(MARGIN_X + 16, y, gname, 1, BLACK);

      // Open count badge (right-aligned)
      int openCnt = countOpenInGroup(g);
      int totCnt  = countTotalInGroup(g);
      char badge[16];
      snprintf(badge, sizeof(badge), "%d/%d", openCnt, totCnt);
      drawStr(EPD_W - MARGIN_X - textW(badge, 1), y, badge, 1, BLACK);

      y += ROW_GROUP;

      // ── Sensor rows ───────────────────────────────────────────────────────
      for (int i = 0; i < discEntityCount && y < MAX_Y; i++) {
        if (discEntities[i].group != g) continue;

        int dotX = MARGIN_X + 6;
        int dotY = y + 6;
        dotStatus(dotX, dotY, discEntities[i].open && discEntities[i].valid);

        // Sensor name (truncated to fit before status label)
        const int NAME_MAX_W = EPD_W - MARGIN_X*2 - 14 - 40;
        drawStrFit(MARGIN_X + 16, y, NAME_MAX_W, discEntities[i].name, 1, BLACK);

        // Status text right-aligned
        if (discEntities[i].valid) {
          const char* status = discEntities[i].open ? "OFFEN" : "zu";
          drawStr(EPD_W - MARGIN_X - textW(status, 1), y, status, 1, BLACK);
        }
        y += ROW_SENSOR;
      }
    }
  }

  // ── IP-Adresse direkt über dem Trennstrich ────────────────────────────────
  // drawStr platziert Text mit y = obere Kante der Bounding-Box.
  // Font-Höhe = 14px → Text bei y=146 belegt y=146..160, Strich bei y=163.
  if (ipStr.length()) {
    drawStr(MARGIN_X, 146, ipStr.c_str(), 1, BLACK);
  }

  // ── Status bar ─────────────────────────────────────────────────────────────
  hline(0, 163, EPD_W, BLACK);
  fillRect(0, 164, EPD_W, EPD_H-164, WHITE);

  // WiFi status
  const char* wifiStr = wifiOk ? "WiFi OK" : "kein WiFi";
  drawStr(MARGIN_X, 180, wifiStr, 1, BLACK);

  if (webActive) drawStr(72, 180, "WEB", 1, BLACK);

  // Time right-aligned
  if (timeStr.length()) {
    drawStr(EPD_W - MARGIN_X - textW(timeStr.c_str(), 1), 180, timeStr.c_str(), 1, BLACK);
  }

  refreshPart();
}

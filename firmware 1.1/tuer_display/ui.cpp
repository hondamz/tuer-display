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

// ── Shared layout helpers ─────────────────────────────────────────────────────

static void drawHeader(const char* text, bool webActive) {
  fillRect(0, 0, EPD_W, 26, BLACK);
  drawStrC(EPD_W/2, 7, text, 1, WHITE);
  if (webActive) {
    fillRect(EPD_W-18, 3, 14, 14, WHITE);
    drawStrC(EPD_W-11, 5, "W", 1, BLACK);
  }
}

static void drawStatusBar(int sepY, const String& timeStr, const String& ipStr,
                          bool wifiOk, bool webActive) {
  // IP above separator
  if (ipStr.length()) {
    drawStr(4, sepY - 17, ipStr.c_str(), 1, BLACK);
  }
  hline(0, sepY, EPD_W, BLACK);
  fillRect(0, sepY+1, EPD_W, EPD_H-(sepY+1), WHITE);
  const char* wifiStr = wifiOk ? "WiFi OK" : "kein WiFi";
  drawStr(4, sepY + 17, wifiStr, 1, BLACK);
  if (webActive) drawStr(72, sepY + 17, "WEB", 1, BLACK);
  if (timeStr.length()) {
    drawStr(EPD_W - 4 - textW(timeStr.c_str(), 1), sepY + 17, timeStr.c_str(), 1, BLACK);
  }
}

static void drawGroupHeader(int& y, int g, bool firstGroup) {
  const int MARGIN_X = 4;
  if (!firstGroup) { hline(MARGIN_X, y-2, EPD_W-2*MARGIN_X, BLACK); }
  if (labelGroups[g].iconType == LG_ICON_WINDOW)    iconWindow(MARGIN_X, y+2, BLACK);
  else if (labelGroups[g].iconType == LG_ICON_DOOR) iconDoor(MARGIN_X, y+2, BLACK);
  else                                               iconGeneric(MARGIN_X, y+2, BLACK);
  char gname[20];
  strlcpy(gname, labelGroups[g].displayName, sizeof(gname));
  drawStr(MARGIN_X + 16, y, gname, 1, BLACK);
  int openCnt = countOpenInGroup(g);
  int totCnt  = countTotalInGroup(g);
  char badge[16];
  snprintf(badge, sizeof(badge), "%d/%d", openCnt, totCnt);
  drawStr(EPD_W - MARGIN_X - textW(badge, 1), y, badge, 1, BLACK);
  y += 19;
}

static void drawSensorRow(int& y, int i) {
  const int MARGIN_X = 4;
  dotStatus(MARGIN_X + 6, y + 6, discEntities[i].open && discEntities[i].valid);
  const int NAME_MAX_W = EPD_W - MARGIN_X*2 - 14 - 40;
  drawStrFit(MARGIN_X + 16, y, NAME_MAX_W, discEntities[i].name, 1, BLACK);
  if (discEntities[i].valid) {
    const char* status = discEntities[i].open ? "OFFEN" : "zu";
    drawStr(EPD_W - MARGIN_X - textW(status, 1), y, status, 1, BLACK);
  }
  y += 16;
}

// ── Screen 0: Übersicht ───────────────────────────────────────────────────────
// Nur Fenster- und Türen-Gruppen; nur offene Sensoren; Separator tiefer (y=170).

void showOverviewScreen(const String& timeStr, const String& ipStr, bool wifiOk, bool webActive) {
  clearWhite();

  int totalOpen = countOpen();
  char hdr[32];
  if (!dwDataValid)       snprintf(hdr, sizeof(hdr), "TUER-DISPLAY");
  else if (totalOpen == 0) snprintf(hdr, sizeof(hdr), "ALLES ZU");
  else if (totalOpen == 1) snprintf(hdr, sizeof(hdr), "1 OFFEN");
  else                     snprintf(hdr, sizeof(hdr), "%d OFFEN", totalOpen);
  drawHeader(hdr, webActive);

  const int SEP_Y = 170;
  const int MAX_Y = SEP_Y - 32; // leave room for IP text (14px) + gap above separator

  int y = 30;
  if (!dwDataValid) {
    drawStrC(EPD_W/2, 100, "Lade Sensoren...", 1, BLACK);
  } else if (discEntityCount == 0) {
    drawStrC(EPD_W/2, 80,  "Keine Sensoren", 1, BLACK);
    drawStrC(EPD_W/2, 96,  "konfiguriert.", 1, BLACK);
    drawStrC(EPD_W/2, 116, "Web-Config: BTN_PWR", 1, BLACK);
  } else {
    bool firstGroup = true;
    for (int g = 0; g < MAX_LABEL_GROUPS && y < MAX_Y; g++) {
      if (!labelGroups[g].used || !labelGroups[g].enabled) continue;
      // Overview: nur Fenster- und Türen-Gruppen
      if (labelGroups[g].iconType != LG_ICON_WINDOW && labelGroups[g].iconType != LG_ICON_DOOR) continue;
      if (countTotalInGroup(g) == 0) continue;

      drawGroupHeader(y, g, firstGroup);
      firstGroup = false;

      // Nur offene Sensoren anzeigen
      for (int i = 0; i < discEntityCount && y < MAX_Y; i++) {
        if (discEntities[i].group != g) continue;
        if (!discEntities[i].valid || !discEntities[i].open) continue;
        drawSensorRow(y, i);
      }
    }
  }

  drawStatusBar(SEP_Y, timeStr, ipStr, wifiOk, webActive);
  refreshPart();
}

// ── Screen 1+: Detailansicht einer Gruppe ─────────────────────────────────────
// Alle Sensoren der Gruppe, Standard-Separator bei y=163.

void showGroupScreen(int groupIdx, const String& timeStr, const String& ipStr,
                    bool wifiOk, bool webActive) {
  clearWhite();

  const int SEP_Y = 163;
  const int MAX_Y = SEP_Y - 20;

  // Header: Gruppenname
  char hdr[24];
  strlcpy(hdr, labelGroups[groupIdx].displayName, sizeof(hdr));
  drawHeader(hdr, webActive);

  int y = 30;
  if (!dwDataValid) {
    drawStrC(EPD_W/2, 100, "Lade Sensoren...", 1, BLACK);
  } else if (countTotalInGroup(groupIdx) == 0) {
    drawStrC(EPD_W/2, 90, "Keine Sensoren", 1, BLACK);
    drawStrC(EPD_W/2, 106, "in dieser Gruppe.", 1, BLACK);
  } else {
    drawGroupHeader(y, groupIdx, true);
    for (int i = 0; i < discEntityCount && y < MAX_Y; i++) {
      if (discEntities[i].group != groupIdx) continue;
      drawSensorRow(y, i);
    }
  }

  drawStatusBar(SEP_Y, timeStr, ipStr, wifiOk, webActive);
  refreshPart();
}

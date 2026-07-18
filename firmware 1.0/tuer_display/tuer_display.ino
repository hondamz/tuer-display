/**
 * Tür-Display v1.0
 *
 * ESP32-S3 (Waveshare 1.54" E-Paper AIoT Board) zeigt den Status aller
 * Türen und Fenster aus Home Assistant auf einem monochromen 200×200px
 * E-Paper-Display an.
 *
 * Funktionsweise:
 *  - Erster Start ohne WLAN-Konfiguration → WiFiManager startet einen
 *    Access-Point "TuerDisplay-Setup". Handy verbinden, Browser öffnen,
 *    WLAN auswählen und Passwort eingeben. Zugangsdaten werden dauerhaft
 *    im Flash gespeichert.
 *  - Nach Verbindung wird alle HA_DEFAULT_INTERVAL Sekunden der Sensor-
 *    Status aus Home Assistant abgerufen (REST-API, Template-Endpoint).
 *  - Sensoren werden über HA-Labels automatisch gefunden (Auto-Discovery).
 *
 * Tasten:
 *  - BTN_REC (GPIO 0)  → Manueller Refresh jetzt
 *  - BTN_PWR (GPIO 18) → Web-Konfigurationsportal ein/aus
 *
 * Abhängigkeiten (über den Arduino-Bibliotheksverwalter installieren):
 *  - WiFiManager by tzapu/tablatronix
 *  - Adafruit GFX Library
 *  - Adafruit BusIO (Abhängigkeit von GFX)
 *
 * Board: esp32:esp32:esp32s3
 * Build-Property: build.partitions=huge_app
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>

#include "config.h"
#include "draw.h"
#include "ha.h"
#include "ui.h"
#include "src/display/epaper_driver_bsp.h"
#include "src/power/board_power_bsp.h"

// ── Globals ───────────────────────────────────────────────────────────────────

epaper_driver_display* display = nullptr;

static board_power_bsp_t* board = nullptr;
static Preferences prefs;
static WebServer   webServer(80);
static bool        webServerRunning = false;
static bool        webServerToggled = false;

// Settings (loaded from NVS in setup())
static String haBaseUrl  = HA_DEFAULT_URL;
static String haToken    = "";
static int    haPollSec  = HA_DEFAULT_INTERVAL;

// Default label group config (user can override via web UI)
static const struct { const char* displayName; const char* haLabel; uint8_t iconType; }
  DEFAULTS[] = {
    { "Fenster", "Fenster", LG_ICON_WINDOW },
    { "Tueren",  "Tuer",    LG_ICON_DOOR   },
  };
static constexpr int DEFAULTS_COUNT = sizeof(DEFAULTS)/sizeof(DEFAULTS[0]);

// State
static bool wifiConnected = false;
static unsigned long lastPollMillis  = 0;
static unsigned long lastDiscMillis  = 0;
static unsigned long lastFullRefresh = 0;
static bool manualRefreshRequested   = false;

// NTP
static const char* NTP_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

// ── Helpers ───────────────────────────────────────────────────────────────────

static String currentTimeStr() {
  struct tm t;
  if (!getLocalTime(&t, 100)) return "";
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  return buf;
}

// ── Settings ─────────────────────────────────────────────────────────────────

static void loadSettings() {
  prefs.begin(NVS_NS, true);
  haBaseUrl = prefs.getString("haUrl",   HA_DEFAULT_URL);
  haToken   = prefs.getString("haToken", "");
  haPollSec = prefs.getInt("haPoll",  HA_DEFAULT_INTERVAL);

  for (int i = 0; i < MAX_LABEL_GROUPS; i++) {
    LabelGroup& g = labelGroups[i];
    if (i < DEFAULTS_COUNT) {
      strlcpy(g.displayName, DEFAULTS[i].displayName, sizeof(g.displayName));
      strlcpy(g.haLabel,     DEFAULTS[i].haLabel,     sizeof(g.haLabel));
      g.iconType = DEFAULTS[i].iconType;
      g.used     = true;
    } else {
      g.displayName[0] = '\0'; g.haLabel[0] = '\0';
      g.iconType = LG_ICON_GENERIC; g.used = false;
    }
    g.enabled = true;

    char k[12];
    snprintf(k, sizeof(k), "lgN%d", i);
    if (prefs.isKey(k)) strlcpy(g.displayName, prefs.getString(k).c_str(), sizeof(g.displayName));
    snprintf(k, sizeof(k), "lgH%d", i);
    if (prefs.isKey(k)) { String s=prefs.getString(k); strlcpy(g.haLabel,s.c_str(),sizeof(g.haLabel)); g.used=(s.length()>0); }
    snprintf(k, sizeof(k), "lgI%d", i);
    if (prefs.isKey(k)) g.iconType = (uint8_t)prefs.getInt(k);
    snprintf(k, sizeof(k), "lgE%d", i);
    if (prefs.isKey(k)) g.enabled = prefs.getBool(k);
  }
  prefs.end();
}

static void saveSettings() {
  prefs.begin(NVS_NS, false);
  prefs.putString("haUrl",   haBaseUrl);
  prefs.putString("haToken", haToken);
  prefs.putInt("haPoll",     haPollSec);
  for (int i=0; i<MAX_LABEL_GROUPS; i++) {
    char k[12];
    snprintf(k,sizeof(k),"lgN%d",i); prefs.putString(k, labelGroups[i].displayName);
    snprintf(k,sizeof(k),"lgH%d",i); prefs.putString(k, labelGroups[i].haLabel);
    snprintf(k,sizeof(k),"lgI%d",i); prefs.putInt(k, labelGroups[i].iconType);
    snprintf(k,sizeof(k),"lgE%d",i); prefs.putBool(k, labelGroups[i].enabled);
  }
  prefs.end();
}

// ── Web configuration server ──────────────────────────────────────────────────

static String htmlHead(const char* title) {
  String s = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'><title>";
  s += title;
  s += "</title><style>"
       "body{font-family:sans-serif;background:#111;color:#eee;padding:16px;max-width:480px;margin:auto}"
       "a{color:#5af}nav a{margin-right:8px}"
       "fieldset{border:1px solid #444;margin-bottom:14px;padding:10px}"
       "label{display:block;margin:8px 0 2px}"
       "input,select{width:100%;padding:6px;box-sizing:border-box;background:#222;color:#eee;border:1px solid #444}"
       "button{margin-top:12px;padding:8px 18px;background:#3a6;color:#fff;border:none;border-radius:4px}"
       "small{color:#999}"
       "</style></head><body>"
       "<nav><a href='/'>Info</a> | <a href='/ha'>Home Assistant</a> | "
       "<a href='/labels'>Labels</a> | <a href='/wifi'>WLAN</a></nav><hr><h2>";
  s += title; s += "</h2>";
  return s;
}

static void handleRoot() {
  String s = htmlHead("Tuer-Display Info");
  s += "<table style='width:100%;border-collapse:collapse'>";
  auto row = [&](const char* k, String v) {
    s += "<tr><td style='padding:4px 6px;border-bottom:1px solid #333'>" + String(k) +
         "</td><td style='padding:4px 6px;border-bottom:1px solid #333'>" + v + "</td></tr>";
  };
  row("Chip",        String(ESP.getChipModel()) + " Rev " + ESP.getChipRevision());
  row("CPU",         String(ESP.getCpuFreqMHz()) + " MHz");
  row("Flash",       String(ESP.getFlashChipSize()/1048576) + " MB");
  row("Freier Heap", String(ESP.getFreeHeap()/1024) + " KB");
  row("MAC",         WiFi.macAddress());
  row("IP",          WiFi.localIP().toString());
  row("SSID",        WiFi.SSID());
  row("RSSI",        String(WiFi.RSSI()) + " dBm");
  row("Sensoren",    String(discEntityCount) + " gefunden, " + countOpen() + " offen");
  row("Laufzeit",    String(millis()/60000) + " min");
  row("Uhrzeit",     currentTimeStr());
  s += "</table>";
  s += "<form method='POST' action='/refresh'><button type='submit'>Jetzt aktualisieren</button></form>";
  s += "</body></html>";
  webServer.send(200, "text/html", s);
}

static void handleRefreshPost() {
  manualRefreshRequested = true;
  webServer.sendHeader("Location", "/"); webServer.send(303);
}

static void handleHaGet() {
  String s = htmlHead("Home Assistant");
  s += "<form method='POST' action='/ha'>"
       "<fieldset><legend>Verbindung</legend>"
       "<label>URL (inkl. Port)</label><input name='url' value='" + haBaseUrl + "'>"
       "<label>Long-Lived Access Token</label><input type='password' name='token' value='" + haToken + "'>"
       "<label>Abfrageintervall (Sekunden)</label>"
       "<input type='number' min='5' name='interval' value='" + haPollSec + "'>"
       "</fieldset>"
       "<button type='submit'>Speichern</button></form>";
  s += "</body></html>";
  webServer.send(200, "text/html", s);
}

static void handleHaPost() {
  haBaseUrl = webServer.arg("url");
  haToken   = webServer.arg("token");
  int iv    = webServer.arg("interval").toInt();
  haPollSec = iv > 0 ? iv : HA_DEFAULT_INTERVAL;
  saveSettings();
  dwDataValid = false;
  discDone    = false;
  lastDiscMillis = 0;
  lastPollMillis = 0;
  webServer.sendHeader("Location", "/ha"); webServer.send(303);
}

static void handleLabelsGet() {
  String s = htmlHead("Labels");
  s += "<p><small>Sensoren werden automatisch ueber HA-Labels gefunden (label_entities). "
       "Leerer HA-Label = Gruppe inaktiv.</small></p>";
  s += "<form method='POST' action='/labels'>";
  for (int i=0; i<MAX_LABEL_GROUPS; i++) {
    s += "<fieldset><legend>Gruppe " + String(i+1);
    if (i < DEFAULTS_COUNT) s += " (Standard)";
    s += "</legend>";
    s += "<label>Anzeigename</label><input name='lgN"+String(i)+"' maxlength='19' value='"+labelGroups[i].displayName+"'>";
    s += "<label>HA-Label</label><input name='lgH"+String(i)+"' maxlength='27' value='"+labelGroups[i].haLabel+"'>";
    s += "<label>Symbol</label><select name='lgI"+String(i)+"'>";
    const char* icons[] = {"Fenster","Tuer","Allgemein"};
    for (int t=0; t<3; t++) {
      s += "<option value='"+String(t)+"'"; if(labelGroups[i].iconType==t) s+=" selected"; s+=">"+String(icons[t])+"</option>";
    }
    s += "</select>";
    s += "<label><input type='checkbox' name='lgE"+String(i)+"' value='1'";
    if (labelGroups[i].enabled) s += " checked";
    s += "> Aktiv</label></fieldset>";
  }
  s += "<button type='submit'>Speichern &amp; Discovery neu starten</button></form>";
  s += "</body></html>";
  webServer.send(200, "text/html", s);
}

static void handleLabelsPost() {
  for (int i=0; i<MAX_LABEL_GROUPS; i++) {
    char k[8];
    snprintf(k,sizeof(k),"lgN%d",i); strlcpy(labelGroups[i].displayName, webServer.arg(k).c_str(), sizeof(labelGroups[i].displayName));
    snprintf(k,sizeof(k),"lgH%d",i); { String h=webServer.arg(k); strlcpy(labelGroups[i].haLabel,h.c_str(),sizeof(labelGroups[i].haLabel)); labelGroups[i].used=(h.length()>0); }
    snprintf(k,sizeof(k),"lgI%d",i); labelGroups[i].iconType = (uint8_t)webServer.arg(k).toInt();
    snprintf(k,sizeof(k),"lgE%d",i); labelGroups[i].enabled = webServer.hasArg(k);
  }
  saveSettings();
  discDone = false; lastDiscMillis = 0;
  webServer.sendHeader("Location", "/labels"); webServer.send(303);
}

static void handleWifiGet() {
  String s = htmlHead("WLAN");
  s += "<p>Verbunden: <b>" + WiFi.SSID() + "</b><br>IP " + WiFi.localIP().toString() + "</p>";
  s += "<p><small>Um das WLAN zu wechseln, Geraet neu starten und zur Setup-IP verbinden, "
       "oder Einstellungen loeschen (Neustart-Taste lange halten).</small></p>";
  s += "<form method='POST' action='/wifi'>"
       "<label>WLAN-Zugangsdaten loeschen und neu konfigurieren</label>"
       "<button type='submit' style='background:#c33'>WLAN zuruecksetzen</button></form>";
  s += "</body></html>";
  webServer.send(200, "text/html", s);
}

static void handleWifiPost() {
  String s = htmlHead("WLAN");
  s += "<p>WLAN-Zugangsdaten geloescht. Geraet startet neu...</p></body></html>";
  webServer.send(200, "text/html", s);
  delay(500);
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

static void startWebServer() {
  webServer.on("/",        HTTP_GET,  handleRoot);
  webServer.on("/refresh", HTTP_POST, handleRefreshPost);
  webServer.on("/ha",      HTTP_GET,  handleHaGet);
  webServer.on("/ha",      HTTP_POST, handleHaPost);
  webServer.on("/labels",  HTTP_GET,  handleLabelsGet);
  webServer.on("/labels",  HTTP_POST, handleLabelsPost);
  webServer.on("/wifi",    HTTP_GET,  handleWifiGet);
  webServer.on("/wifi",    HTTP_POST, handleWifiPost);
  webServer.begin();
  webServerRunning = true;
  Serial.printf("[WEB] Gestartet auf http://%s/\n", WiFi.localIP().toString().c_str());
}

static void stopWebServer() {
  webServer.stop();
  webServerRunning = false;
}

// ── EPD setup ─────────────────────────────────────────────────────────────────

static void initDisplay() {
  board = new board_power_bsp_t(EPD_PWR_PIN, AUDIO_PWR_PIN, VBAT_PWR_PIN);
  board->POWEER_EPD_ON();
  delay(100);

  custom_lcd_spi_t spi_cfg = {
    .cs       = (uint8_t)EPD_CS_PIN,
    .dc       = (uint8_t)EPD_DC_PIN,
    .rst      = (uint8_t)EPD_RST_PIN,
    .busy     = (uint8_t)EPD_BUSY_PIN,
    .mosi     = (uint8_t)EPD_MOSI_PIN,
    .scl      = (uint8_t)EPD_SCK_PIN,
    .spi_host = EPD_SPI_NUM,
    .buffer_len = EPD_WIDTH * EPD_HEIGHT / 8,
  };
  display = new epaper_driver_display(EPD_WIDTH, EPD_HEIGHT, spi_cfg);
  display->EPD_Init();
  display->EPD_DisplayPartBaseImage();
  display->EPD_Init_Partial();
}

// ── Poll logic ────────────────────────────────────────────────────────────────

static void pollHA() {
  if (!wifiConnected) return;

  unsigned long now = millis();
  bool discDue = (lastDiscMillis == 0 || now - lastDiscMillis >= DISCOVER_INTERVAL_MS);
  if (discDue) {
    Serial.println("[HA] Starte Discovery...");
    haDiscoverLabels(haBaseUrl, haToken);
    lastDiscMillis = millis();
  }

  if (!discDone || discEntityCount == 0) return;

  bool pollDue = (lastPollMillis == 0 || now - lastPollMillis >= (unsigned long)haPollSec * 1000UL);
  if (!pollDue && !manualRefreshRequested) return;

  manualRefreshRequested = false;
  lastPollMillis = millis();

  Serial.println("[HA] Hole Sensor-Status...");
  if (haFetchDwStates(haBaseUrl, haToken)) {
    dwDataValid = true;
    Serial.printf("[HA] %d offen\n", countOpen());

    // Full refresh every hour to avoid ghosting; partial otherwise
    bool doFull = (now - lastFullRefresh >= FULL_REFRESH_INTERVAL_MS);
    if (doFull) {
      display->EPD_Init();
      display->EPD_Display();
      display->EPD_DisplayPartBaseImage();
      display->EPD_Init_Partial();
      lastFullRefresh = millis();
    }
    showDoorWindowScreen(currentTimeStr(), wifiConnected, webServerRunning);
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[BOOT] Tuer-Display v1.0");

  initDisplay();
  showBoot("Starte...");

  loadSettings();

  // Button pins
  pinMode(BTN_REC, INPUT_PULLUP);
  pinMode(BTN_PWR, INPUT_PULLUP);

  // ── WiFiManager ──────────────────────────────────────────────────────────
  showBoot("Verbinde WLAN...");

  WiFiManager wm;
  wm.setConfigPortalTimeout(WIFI_TIMEOUT_S);

  // Show AP info on display as soon as portal starts
  wm.setAPCallback([](WiFiManager* wm) {
    showWifiPortal(WIFI_AP_NAME, "192.168.4.1");
  });

  bool connected = wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);

  if (!connected) {
    Serial.println("[WiFi] Kein WLAN – starte ohne Netzwerk");
    showError("Kein WLAN.\nBTN_PWR = Web-Portal");
    wifiConnected = false;
  } else {
    wifiConnected = true;
    Serial.printf("[WiFi] Verbunden: %s / %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    showConnecting(WiFi.SSID().c_str());

    // NTP
    configTzTime(NTP_TZ, "pool.ntp.org", "time.nist.gov");
    delay(1500);

    // Initial data fetch
    showBoot("Lade Sensoren...");
    haDiscoverLabels(haBaseUrl, haToken);
    lastDiscMillis = millis();
    if (discDone && discEntityCount > 0) {
      haFetchDwStates(haBaseUrl, haToken);
      if (dwDataValid) {
        display->EPD_Init();
        display->EPD_Display();
        display->EPD_DisplayPartBaseImage();
        display->EPD_Init_Partial();
        lastFullRefresh = millis();
      }
    }
    showDoorWindowScreen(currentTimeStr(), wifiConnected, false);
  }
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
  // BTN_REC: manual refresh
  if (digitalRead(BTN_REC) == LOW) {
    delay(50);
    if (digitalRead(BTN_REC) == LOW) {
      while (digitalRead(BTN_REC) == LOW) delay(10);
      Serial.println("[BTN] Manueller Refresh");
      manualRefreshRequested = true;
    }
  }

  // BTN_PWR: toggle web server
  if (digitalRead(BTN_PWR) == LOW) {
    delay(50);
    if (digitalRead(BTN_PWR) == LOW) {
      while (digitalRead(BTN_PWR) == LOW) delay(10);
      if (wifiConnected) {
        if (webServerRunning) {
          stopWebServer();
          Serial.println("[WEB] Gestoppt");
        } else {
          startWebServer();
        }
        showDoorWindowScreen(currentTimeStr(), wifiConnected, webServerRunning);
      }
    }
  }

  if (webServerRunning) webServer.handleClient();

  pollHA();

  delay(100);
}

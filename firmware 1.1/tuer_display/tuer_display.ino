/**
 * Tür-Display v1.1
 *
 * ESP32-S3 (Waveshare 1.54" E-Paper AIoT Board) zeigt den Status aller
 * Türen und Fenster aus Home Assistant auf einem monochromen 200×200px
 * E-Paper-Display an.
 *
 * Funktionsweise:
 *  - Beim Start wird versucht, das gespeicherte WLAN automatisch zu
 *    verbinden (60 Sekunden Timeout). Schlägt dies fehl, kann der Anwender
 *    wählen: nochmals versuchen oder den WiFiManager-Portal starten.
 *  - Nach Verbindung wird alle HA_DEFAULT_INTERVAL Sekunden der Sensor-
 *    Status aus Home Assistant abgerufen (REST-API, Template-Endpoint).
 *  - Sensoren werden über HA-Labels automatisch gefunden (Auto-Discovery).
 *  - Sensor-Anzeigenamen können über das Web-Portal überschrieben werden.
 *
 * Tasten:
 *  - BTN_REC (GPIO 0)  → Manueller Refresh
 *  - BTN_PWR (GPIO 18) → Kurz: Web-Portal ein/aus
 *                        Lang (>500ms): Einstellungsmenü
 *
 * Einstellungsmenü (E-Paper):
 *  - Zeigt WLAN-Info (SSID, IP, Signalstärke)
 *  - REC: Web-Konfig umschalten
 *  - PWR: Zurück zur Hauptanzeige
 *
 * Web-Portal (http://<ip>/):
 *  - /       Geräteinformationen, manueller Refresh
 *  - /ha     HA URL, Access Token, Abfrageintervall
 *  - /labels Label-Gruppen konfigurieren
 *  - /sensors Sensor-Anzeigenamen überschreiben
 *  - /wifi   WLAN-Status, WLAN neu einrichten
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

// Settings (loaded from NVS in setup())
static String haBaseUrl  = HA_DEFAULT_URL;
static String haToken    = "";
static int    haPollSec  = HA_DEFAULT_INTERVAL;

// WiFi / Netzwerk-Einstellungen
static String wHostname   = "HATUERESTATUS";
static bool   wStaticIp   = false;
static String wStaticAddr = "";
static String wStaticMask = "255.255.255.0";
static String wStaticGw   = "";
static String wStaticDns  = "";

// Default label group config (user can override via web UI)
static const struct { const char* displayName; const char* haLabel; uint8_t iconType; }
  DEFAULTS[] = {
    { "Fenster", "Fenster", LG_ICON_WINDOW },
    { "Tueren",  "Tuer",    LG_ICON_DOOR   },
  };
static constexpr int DEFAULTS_COUNT = sizeof(DEFAULTS)/sizeof(DEFAULTS[0]);

// ── Deep-Sleep-persistente Variablen (RTC-RAM, überleben Deep Sleep) ──────────

struct StateSnap { bool open; bool valid; };

RTC_DATA_ATTR static int        rtcScreen             = 0;
RTC_DATA_ATTR static char       rtcChangeTime[8]      = "";
RTC_DATA_ATTR static StateSnap  rtcSnap[MAX_DISC_ENTITIES];
RTC_DATA_ATTR static int        rtcSnapCount          = -1; // -1 = noch nie gezeichnet
RTC_DATA_ATTR static DiscEntity rtcEntities[MAX_DISC_ENTITIES];
RTC_DATA_ATTR static int        rtcEntityCount        = 0;
RTC_DATA_ATTR static bool       rtcDiscDone           = false;
RTC_DATA_ATTR static int        rtcPollsSinceDiscover = 999; // hoch = Discovery beim 1. Wake
RTC_DATA_ATTR static int        rtcPollsSinceFull     = 0;

// ── State (RAM, wird bei jedem Wake neu gesetzt) ──────────────────────────────

static bool   wifiConnected    = false;
static int    currentScreen    = 0;
static String lastChangeTimeStr = "";

// ── Snapshot / RTC Helpers ────────────────────────────────────────────────────

static bool statesChanged() {
  if (discEntityCount != rtcSnapCount) return true;
  for (int i = 0; i < discEntityCount; i++) {
    if (discEntities[i].open  != rtcSnap[i].open ||
        discEntities[i].valid != rtcSnap[i].valid) return true;
  }
  return false;
}

static void saveSnapshot() {
  rtcSnapCount = discEntityCount;
  for (int i = 0; i < discEntityCount; i++) {
    rtcSnap[i].open  = discEntities[i].open;
    rtcSnap[i].valid = discEntities[i].valid;
  }
}

static void loadFromRtc() {
  currentScreen     = rtcScreen;
  lastChangeTimeStr = rtcChangeTime;
  discEntityCount   = rtcEntityCount;
  discDone          = rtcDiscDone;
  dwDataValid       = (rtcSnapCount >= 0);
  memcpy(discEntities, rtcEntities, sizeof(DiscEntity) * MAX_DISC_ENTITIES);
}

static void saveToRtc() {
  rtcScreen      = currentScreen;
  strlcpy(rtcChangeTime, lastChangeTimeStr.c_str(), sizeof(rtcChangeTime));
  rtcEntityCount = discEntityCount;
  rtcDiscDone    = discDone;
  memcpy(rtcEntities, discEntities, sizeof(DiscEntity) * MAX_DISC_ENTITIES);
}

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

  // WiFi / Netzwerk
  wHostname   = prefs.getString("wHost",  "HATUERESTATUS");
  wStaticIp   = prefs.getBool("wStatic",  false);
  wStaticAddr = prefs.getString("wAddr",  "");
  wStaticMask = prefs.getString("wMask",  "255.255.255.0");
  wStaticGw   = prefs.getString("wGw",    "");
  wStaticDns  = prefs.getString("wDns",   "");

  // Load sensor name overrides ("entityId|name~entityId|name~...")
  nameOverrideCount = 0;
  String nmOv = prefs.getString("nmOv", "");
  int start = 0;
  while (start < (int)nmOv.length() && nameOverrideCount < MAX_DISC_ENTITIES) {
    int tilde = nmOv.indexOf('~', start);
    String pair = (tilde < 0) ? nmOv.substring(start) : nmOv.substring(start, tilde);
    int pipe = pair.indexOf('|');
    if (pipe > 0) {
      strlcpy(nameOverrides[nameOverrideCount].entityId,    pair.substring(0, pipe).c_str(), sizeof(nameOverrides[0].entityId));
      strlcpy(nameOverrides[nameOverrideCount].displayName, pair.substring(pipe+1).c_str(),  sizeof(nameOverrides[0].displayName));
      nameOverrideCount++;
    }
    if (tilde < 0) break;
    start = tilde + 1;
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
  // Save sensor name overrides
  String nmOv = "";
  for (int i=0; i<nameOverrideCount; i++) {
    if (i) nmOv += "~";
    nmOv += nameOverrides[i].entityId;
    nmOv += "|";
    nmOv += nameOverrides[i].displayName;
  }
  prefs.putString("nmOv", nmOv);
  // WiFi / Netzwerk
  prefs.putString("wHost",  wHostname);
  prefs.putBool("wStatic",  wStaticIp);
  prefs.putString("wAddr",  wStaticAddr);
  prefs.putString("wMask",  wStaticMask);
  prefs.putString("wGw",    wStaticGw);
  prefs.putString("wDns",   wStaticDns);
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
       "<nav><a href='/'>Info</a> | <a href='/ha'>HA</a> | "
       "<a href='/labels'>Labels</a> | <a href='/sensors'>Sensoren</a> | "
       "<a href='/wifi'>WLAN</a></nav><hr><h2>";
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
  row("Version",     "1.1");
  row("Hostname",    wHostname);
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
  // Im Web-Portal-Modus: sofort Poll anstoßen
  doHaPoll(true);
  saveToRtc();
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
  rtcPollsSinceDiscover = 999; // Discovery beim nächsten Poll erzwingen
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
  discDone = false;
  rtcPollsSinceDiscover = 999;
  webServer.sendHeader("Location", "/labels"); webServer.send(303);
}

static void handleSensorsGet() {
  String s = htmlHead("Sensoren");
  s += "<style>"
       ".dot{font-size:1.3em;line-height:1;vertical-align:middle;margin-right:6px}"
       ".open{color:#e44}.closed{color:#4c4}.unknown{color:#a4a}"
       "</style>";
  if (discEntityCount == 0) {
    s += "<p>Noch keine Sensoren entdeckt. HA konfigurieren und kurz warten.</p>";
  } else {
    s += "<p><small>Farbpunkt: <span class='dot open'>●</span>offen &nbsp;"
         "<span class='dot closed'>●</span>geschlossen &nbsp;"
         "<span class='dot unknown'>●</span>unbekannt</small></p>";
    s += "<p><small>Anzeigenamen fuer das Display anpassen. Leer = HA-Name.</small></p>";
    s += "<form method='POST' action='/sensors'>"
         "<input type='hidden' name='cnt' value='" + String(discEntityCount) + "'>";
    for (int i=0; i<discEntityCount; i++) {
      const char* dotClass = !discEntities[i].valid ? "unknown"
                           : discEntities[i].open   ? "open" : "closed";
      s += "<fieldset><legend>";
      s += "<span class='dot " + String(dotClass) + "'>&#9679;</span>";
      s += "<small>" + String(discEntities[i].entityId) + "</small></legend>";
      s += "<label>Anzeigename (max. 27 Zeichen)</label>";
      s += "<input name='n"+String(i)+"' maxlength='27' value='"+String(discEntities[i].name)+"'>";
      s += "<input type='hidden' name='e"+String(i)+"' value='"+String(discEntities[i].entityId)+"'>";
      s += "</fieldset>";
    }
    s += "<button type='submit'>Speichern</button></form>";
  }
  s += "</body></html>";
  webServer.send(200, "text/html", s);
}

static void handleSensorsPost() {
  int cnt = webServer.arg("cnt").toInt();
  nameOverrideCount = 0;
  for (int i=0; i<cnt && i<MAX_DISC_ENTITIES; i++) {
    String eid  = webServer.arg("e" + String(i));
    String name = webServer.arg("n" + String(i));
    name.trim();
    if (eid.length() == 0 || name.length() == 0) continue;
    strlcpy(nameOverrides[nameOverrideCount].entityId,    eid.c_str(),  sizeof(nameOverrides[0].entityId));
    strlcpy(nameOverrides[nameOverrideCount].displayName, name.c_str(), sizeof(nameOverrides[0].displayName));
    nameOverrideCount++;
  }
  saveSettings();
  applyNameOverrides();
  webServer.sendHeader("Location", "/sensors"); webServer.send(303);
}

static void handleWifiScan() {
  int n = WiFi.scanNetworks(false, true); // sync, include hidden
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i) json += ",";
    String ssid = WiFi.SSID(i);
    ssid.replace("\"", "\\\"");
    json += "{\"s\":\"" + ssid + "\",\"r\":" + WiFi.RSSI(i) +
            ",\"e\":" + (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "1" : "0") + "}";
  }
  json += "]";
  WiFi.scanDelete();
  webServer.sendHeader("Access-Control-Allow-Origin", "*");
  webServer.send(200, "application/json", json);
}

static void handleWifiGet() {
  String s = htmlHead("WLAN");

  // ── Aktuelle Verbindung ──────────────────────────────────────────────────
  s += "<h3>Aktuelle Verbindung</h3>"
       "<table style='width:100%;border-collapse:collapse'>";
  auto row = [&](const char* k, String v) {
    s += "<tr><td style='padding:3px 6px;border-bottom:1px solid #333;width:40%'>" + String(k) +
         "</td><td style='padding:3px 6px;border-bottom:1px solid #333'>" + v + "</td></tr>";
  };
  row("Hostname",  wHostname);
  row("SSID",      WiFi.SSID());
  row("Signal",    String(WiFi.RSSI()) + " dBm");
  row("IP-Modus",  wStaticIp ? "Fest" : "DHCP");
  row("IP",        WiFi.localIP().toString());
  row("Subnetz",   WiFi.subnetMask().toString());
  row("Gateway",   WiFi.gatewayIP().toString());
  row("DNS",       WiFi.dnsIP().toString());
  row("MAC",       WiFi.macAddress());
  s += "</table>";

  // ── Konfigurationsformular ───────────────────────────────────────────────
  s += "<h3>Konfiguration</h3>"
       "<form method='POST' action='/wifi'>"

       "<fieldset><legend>Hostname</legend>"
       "<label>Geraetename im Netzwerk</label>"
       "<input name='hostname' maxlength='32' value='" + wHostname + "'>"
       "</fieldset>"

       "<fieldset><legend>WLAN wechseln</legend>"
       "<label>Verfuegbare Netze</label>"
       "<div style='display:flex;gap:6px'>"
       "<select id='ssidSel' style='flex:1' onchange=\"document.getElementById('ssidIn').value=this.value\">"
       "<option value=''>-- Scan starten --</option></select>"
       "<button type='button' onclick='doScan()' style='flex:none;margin:0'>Suchen</button>"
       "</div>"
       "<label>SSID</label>"
       "<input id='ssidIn' name='ssid' value=''>"
       "<label>Passwort</label>"
       "<input type='password' name='pass'>"
       "<small>Leer lassen = unveraendert (nur bei SSID-Wechsel noetig)</small>"
       "</fieldset>"

       "<fieldset><legend>IP-Konfiguration</legend>"
       "<label style='display:flex;align-items:center;gap:8px'>"
       "<input type='checkbox' name='staticIp' id='chkStatic' value='1'";
  if (wStaticIp) s += " checked";
  s += " onchange='toggleStatic(this.checked)'> Feste IP-Adresse verwenden</label>"
       "<div id='staticFields' style='display:" + String(wStaticIp ? "block" : "none") + "'>"
       "<label>IP-Adresse</label><input name='ip'   value='" + wStaticAddr + "'>"
       "<label>Subnetzmaske</label><input name='mask' value='" + wStaticMask + "'>"
       "<label>Gateway</label><input name='gw'   value='" + wStaticGw   + "'>"
       "<label>DNS-Server</label><input name='dns'  value='" + wStaticDns  + "'>"
       "</div></fieldset>"

       "<button type='submit'>Speichern &amp; Neustart</button>"
       "</form>"

       "<script>"
       "function toggleStatic(on){document.getElementById('staticFields').style.display=on?'block':'none';}"
       "async function doScan(){"
       "  var sel=document.getElementById('ssidSel');"
       "  sel.innerHTML='<option>Suche...';"
       "  var r=await fetch('/wifi/scan');"
       "  var nets=await r.json();"
       "  nets.sort((a,b)=>b.r-a.r);"
       "  sel.innerHTML=nets.map(n=>"
       "    '<option value=\"'+n.s+'\">'+(n.e?'🔒 ':'')+n.s+' ('+n.r+' dBm)</option>'"
       "  ).join('');"
       "  if(nets.length>0)document.getElementById('ssidIn').value=nets[0].s;"
       "}"
       "</script>"
       "</body></html>";
  webServer.send(200, "text/html", s);
}

static void handleWifiPost() {
  String newHostname = webServer.arg("hostname"); newHostname.trim();
  String newSsid     = webServer.arg("ssid");     newSsid.trim();
  String newPass     = webServer.arg("pass");
  bool   newStatic   = webServer.hasArg("staticIp");
  String newIp       = webServer.arg("ip");   newIp.trim();
  String newMask     = webServer.arg("mask"); newMask.trim();
  String newGw       = webServer.arg("gw");   newGw.trim();
  String newDns      = webServer.arg("dns");  newDns.trim();

  if (newHostname.length() > 0) wHostname = newHostname;
  wStaticIp   = newStatic;
  wStaticAddr = newIp;
  if (newMask.length() > 0) wStaticMask = newMask;
  wStaticGw   = newGw;
  wStaticDns  = newDns;
  saveSettings();

  String s = htmlHead("WLAN");
  s += "<p>Einstellungen gespeichert. Geraet startet neu...</p></body></html>";
  webServer.send(200, "text/html", s);
  delay(500);

  // Neue WLAN-Credentials direkt setzen, dann Neustart
  if (newSsid.length() > 0) {
    WiFi.begin(newSsid.c_str(), newPass.length() > 0 ? newPass.c_str() : nullptr);
    delay(200);
  }
  ESP.restart();
}

static void startWebServer() {
  webServer.on("/",        HTTP_GET,  handleRoot);
  webServer.on("/refresh", HTTP_POST, handleRefreshPost);
  webServer.on("/ha",      HTTP_GET,  handleHaGet);
  webServer.on("/ha",      HTTP_POST, handleHaPost);
  webServer.on("/labels",  HTTP_GET,  handleLabelsGet);
  webServer.on("/labels",  HTTP_POST, handleLabelsPost);
  webServer.on("/sensors", HTTP_GET,  handleSensorsGet);
  webServer.on("/sensors", HTTP_POST, handleSensorsPost);
  webServer.on("/wifi",      HTTP_GET,  handleWifiGet);
  webServer.on("/wifi",      HTTP_POST, handleWifiPost);
  webServer.on("/wifi/scan", HTTP_GET,  handleWifiScan);
  webServer.begin();
  webServerRunning = true;
  Serial.printf("[WEB] Gestartet auf http://%s/\n", WiFi.localIP().toString().c_str());
}

static void stopWebServer() {
  webServer.stop();
  webServerRunning = false;
}

// ── Screen helpers ────────────────────────────────────────────────────────────

static int countDetailScreens() {
  int n = 0;
  for (int g = 0; g < MAX_LABEL_GROUPS; g++) {
    if (labelGroups[g].used && labelGroups[g].enabled && countTotalInGroup(g) > 0) n++;
  }
  return n;
}

static int groupForScreen(int screenIdx) {
  int n = 0;
  for (int g = 0; g < MAX_LABEL_GROUPS; g++) {
    if (!labelGroups[g].used || !labelGroups[g].enabled || countTotalInGroup(g) == 0) continue;
    n++;
    if (n == screenIdx) return g;
  }
  return -1;
}

static void showCurrentScreen() {
  String ip = wifiConnected ? WiFi.localIP().toString() : "";
  if (currentScreen == 0) {
    showOverviewScreen(lastChangeTimeStr, ip, wifiConnected, webServerRunning);
  } else {
    int g = groupForScreen(currentScreen);
    if (g >= 0) showGroupScreen(g, lastChangeTimeStr, ip, wifiConnected, webServerRunning);
    else        showOverviewScreen(lastChangeTimeStr, ip, wifiConnected, webServerRunning);
  }
}

// ── EPD helper ────────────────────────────────────────────────────────────────

static void reInitPartialAndShow() {
  display->EPD_Init();
  display->EPD_Display();
  display->EPD_DisplayPartBaseImage();
  display->EPD_Init_Partial();
  rtcPollsSinceFull = 0;
  showCurrentScreen();
}

// ── Sleep ─────────────────────────────────────────────────────────────────────

static void goToSleep() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  uint64_t btnMask = (1ULL << BTN_REC) | (1ULL << BTN_PWR);
  esp_sleep_enable_ext1_wakeup(btnMask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_sleep_enable_timer_wakeup((uint64_t)haPollSec * 1000000ULL);
  Serial.printf("[SLEEP] %ds bis naechster Poll\n", haPollSec);
  Serial.flush();
  esp_deep_sleep_start();
}

// ── HA-Poll (einmalig, wird direkt aus setup() aufgerufen) ───────────────────

static void doHaPoll(bool forceRedraw) {
  // Discovery wenn nötig
  int discIntervalPolls = max(1, (int)(DISCOVER_INTERVAL_MS / 1000 / haPollSec));
  if (rtcPollsSinceDiscover >= discIntervalPolls || !rtcDiscDone) {
    Serial.println("[HA] Discovery...");
    haDiscoverLabels(haBaseUrl, haToken);
    applyNameOverrides();
    rtcPollsSinceDiscover = 0;
  } else {
    rtcPollsSinceDiscover++;
  }

  if (!discDone || discEntityCount == 0) return;

  Serial.println("[HA] Hole Status...");
  if (!haFetchDwStates(haBaseUrl, haToken)) return;
  dwDataValid = true;

  bool changed = statesChanged();
  if (changed) {
    saveSnapshot();
    lastChangeTimeStr = currentTimeStr();
    Serial.printf("[HA] Aenderung: %d offen, Zeit %s\n", countOpen(), lastChangeTimeStr.c_str());
  } else {
    Serial.println("[HA] Keine Aenderung");
  }

  rtcPollsSinceFull++;
  int fullIntervalPolls = max(1, (int)(FULL_REFRESH_INTERVAL_MS / 1000 / haPollSec));
  bool doFull = (rtcPollsSinceFull >= fullIntervalPolls);

  if (doFull)                 reInitPartialAndShow();
  else if (changed || forceRedraw) showCurrentScreen();
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

// ── Settings menu (blocking) ──────────────────────────────────────────────────

static void enterSettings() {
  showSettings(WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI(), webServerRunning);

  while (true) {
    // BTN_REC: toggle web portal
    if (digitalRead(BTN_REC) == LOW) {
      delay(50);
      if (digitalRead(BTN_REC) == LOW) {
        while (digitalRead(BTN_REC) == LOW) delay(10);
        if (wifiConnected) {
          if (webServerRunning) stopWebServer();
          else                  startWebServer();
        }
        showSettings(WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI(), webServerRunning);
      }
    }

    // BTN_PWR: exit settings
    if (digitalRead(BTN_PWR) == LOW) {
      delay(50);
      if (digitalRead(BTN_PWR) == LOW) {
        while (digitalRead(BTN_PWR) == LOW) delay(10);
        break;
      }
    }

    if (webServerRunning) webServer.handleClient();
    delay(50);
  }

  // Vollbild-Refresh nötig, um Partial-Modus wieder korrekt zu initialisieren
  reInitPartialAndShow();
}

// ── WiFi boot ─────────────────────────────────────────────────────────────────

// Schnelle, stille Verbindung für Timer-Wakes (kein Display-Update).
// Gibt true zurück wenn verbunden, false nach 15s Timeout.
static bool quickConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(wHostname.c_str());
  if (wStaticIp && wStaticAddr.length() > 0) {
    IPAddress ip, mask, gw, dns;
    ip.fromString(wStaticAddr);
    mask.fromString(wStaticMask.length() > 0 ? wStaticMask : "255.255.255.0");
    gw.fromString(wStaticGw);
    dns.fromString(wStaticDns.length() > 0 ? wStaticDns : wStaticGw);
    WiFi.config(ip, gw, mask, dns);
  }
  WiFi.begin();
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(200);
  bool ok = (WiFi.status() == WL_CONNECTED);
  Serial.printf("[WiFi] %s\n", ok ? WiFi.localIP().toString().c_str() : "Timeout");
  return ok;
}
// Versucht gespeicherte WLAN-Zugangsdaten. Schlägt das 60s-Timeout fehl,
// kann der Anwender per Taste wiederholen oder den WiFiManager starten.
// Gibt true zurück, wenn eine Verbindung besteht.
static bool connectWifi() {
  // WiFi.SSID() liefert vor WiFi.begin() immer leer — erst nach begin()
  // sind die NVS-Zugangsdaten geladen.
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(wHostname.c_str());

  // Statische IP konfigurieren, wenn gewünscht
  if (wStaticIp && wStaticAddr.length() > 0) {
    IPAddress ip, mask, gw, dns;
    ip.fromString(wStaticAddr);
    mask.fromString(wStaticMask.length() > 0 ? wStaticMask : "255.255.255.0");
    gw.fromString(wStaticGw);
    dns.fromString(wStaticDns.length() > 0 ? wStaticDns : wStaticGw);
    WiFi.config(ip, gw, mask, dns);
  }

  WiFi.begin(); // NVS-Zugangsdaten laden, Verbindung starten

  // Warten bis SSID aus NVS gelesen ist (kann >500ms dauern)
  unsigned long t0 = millis();
  while (WiFi.SSID().length() == 0 && millis() - t0 < 3000) delay(100);

  String savedSsid = WiFi.SSID();

  if (savedSsid.length() == 0) {
    // Noch keine gespeicherten Zugangsdaten → direkt WiFiManager
    WiFi.disconnect(true);
    showBoot("WLAN einrichten...");
    WiFiManager wm;
    wm.setConfigPortalTimeout(WIFI_TIMEOUT_S);
    wm.setAPCallback([](WiFiManager*) { showWifiPortal(WIFI_AP_NAME, "192.168.4.1"); });
    return wm.startConfigPortal(WIFI_AP_NAME, WIFI_AP_PASSWORD);
  }

  while (true) {
    showConnecting(savedSsid.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) return true;

    // Timeout — Anwender fragen
    showWifiFail(savedSsid.c_str());

    bool retry = false;
    bool setup = false;
    while (!retry && !setup) {
      if (digitalRead(BTN_REC) == LOW) {
        delay(50);
        if (digitalRead(BTN_REC) == LOW) { while (digitalRead(BTN_REC) == LOW) delay(10); retry = true; }
      }
      if (digitalRead(BTN_PWR) == LOW) {
        delay(50);
        if (digitalRead(BTN_PWR) == LOW) { while (digitalRead(BTN_PWR) == LOW) delay(10); setup = true; }
      }
      delay(50);
    }

    if (setup) {
      WiFiManager wm;
      wm.setConfigPortalTimeout(WIFI_TIMEOUT_S);
      wm.setAPCallback([](WiFiManager*) { showWifiPortal(WIFI_AP_NAME, "192.168.4.1"); });
      bool ok = wm.startConfigPortal(WIFI_AP_NAME, WIFI_AP_PASSWORD);
      return ok;
    }
    // retry → WiFi.begin() neu starten und Schleife wiederholen
    WiFi.begin();
    unsigned long tr = millis();
    while (WiFi.SSID().length() == 0 && millis() - tr < 3000) delay(100);
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(100);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  Serial.printf("\n[BOOT] Tuer-Display v1.1 – Wakeup: %d\n", cause);

  pinMode(BTN_REC, INPUT_PULLUP);
  pinMode(BTN_PWR, INPUT_PULLUP);

  loadSettings();
  initDisplay();

  // ── Erster Start ────────────────────────────────────────────────────────────
  if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    showBoot("Starte...");
    wifiConnected = connectWifi();

    clearWhite();
    display->EPD_Init();
    display->EPD_Display();
    display->EPD_DisplayPartBaseImage();
    display->EPD_Init_Partial();

    if (!wifiConnected) {
      showError("Kein WLAN. BTN_PWR lang = Einstellungen");
      saveToRtc();
      goToSleep();
      return;
    }

    Serial.printf("[WiFi] %s / %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    configTzTime(NTP_TZ, "pool.ntp.org", "time.nist.gov");
    delay(1500);

    showBoot("Lade Sensoren...");
    rtcPollsSinceDiscover = 999; // Discovery erzwingen
    doHaPoll(true);
    saveToRtc();
    showCurrentScreen();
    goToSleep();
    return;
  }

  // ── Wake durch Timer (normaler Poll-Zyklus) ─────────────────────────────────
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    loadFromRtc();
    // Stille Verbindung: kein showConnecting(), Display bleibt unberührt
    wifiConnected = quickConnect();
    if (wifiConnected) {
      configTzTime(NTP_TZ, "pool.ntp.org", "time.nist.gov");
      delay(300);
      doHaPoll(false);
    }
    saveToRtc();
    goToSleep();
    return;
  }

  // ── Wake durch Taste (EXT1) ─────────────────────────────────────────────────
  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    loadFromRtc();

    uint64_t pins = esp_sleep_get_ext1_wakeup_status();
    bool recWoke  = pins & (1ULL << BTN_REC);

    // Tastendauer messen (Taste könnte noch gedrückt sein)
    delay(30);
    unsigned long pressStart = millis();
    int   activePin = recWoke ? BTN_REC : BTN_PWR;
    while (digitalRead(activePin) == LOW) delay(10);
    unsigned long pressDur = millis() - pressStart;
    bool longPress = (pressDur >= WIFI_LONG_PRESS_MS);

    if (recWoke) {
      // Screen wechseln + frischen Poll holen (still, kein Connecting-Screen)
      wifiConnected = quickConnect();
      if (wifiConnected) {
        configTzTime(NTP_TZ, "pool.ntp.org", "time.nist.gov");
        delay(300);
        doHaPoll(false);
      }
      int total = 1 + countDetailScreens();
      if (total > 1) currentScreen = (currentScreen + 1) % total;
      showCurrentScreen(); // zeigt immer den neuen Screen
      saveToRtc();
      goToSleep();
      return;
    }

    // BTN_PWR
    wifiConnected = connectWifi();
    if (!wifiConnected) { goToSleep(); return; }

    configTzTime(NTP_TZ, "pool.ntp.org", "time.nist.gov");
    delay(300);
    doHaPoll(false);

    clearWhite();
    display->EPD_Init();
    display->EPD_Display();
    display->EPD_DisplayPartBaseImage();
    display->EPD_Init_Partial();

    if (longPress) {
      enterSettings(); // blocking; exitiert via reInitPartialAndShow()
      saveToRtc();
      if (!webServerRunning) { goToSleep(); return; }
      // Web-Portal läuft: in loop() weitermachen
    } else {
      startWebServer();
      showCurrentScreen();
      // in loop() weitermachen
    }
    saveToRtc();
    return; // → loop()
  }

  // Unbekannter Wake-Grund: schlafen
  goToSleep();
}

// ── Loop — läuft nur wenn Web-Portal aktiv ────────────────────────────────────

void loop() {
  if (!webServerRunning) { goToSleep(); return; }

  webServer.handleClient();

  // BTN_REC: manuellen Refresh holen
  if (digitalRead(BTN_REC) == LOW) {
    delay(50);
    if (digitalRead(BTN_REC) == LOW) {
      while (digitalRead(BTN_REC) == LOW) delay(10);
      doHaPoll(true);
      saveToRtc();
    }
  }

  // BTN_PWR: kurz = Portal stoppen + schlafen, lang = Einstellungsmenü
  if (digitalRead(BTN_PWR) == LOW) {
    delay(50);
    if (digitalRead(BTN_PWR) == LOW) {
      unsigned long pressStart = millis();
      while (digitalRead(BTN_PWR) == LOW) delay(10);
      if (millis() - pressStart >= WIFI_LONG_PRESS_MS) {
        enterSettings();
        saveToRtc();
        if (!webServerRunning) { goToSleep(); return; }
      } else {
        stopWebServer();
        saveToRtc();
        goToSleep();
        return;
      }
    }
  }

  delay(20);
}

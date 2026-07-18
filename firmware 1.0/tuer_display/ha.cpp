#include "ha.h"
#include <HTTPClient.h>

LabelGroup  labelGroups[MAX_LABEL_GROUPS];
DiscEntity  discEntities[MAX_DISC_ENTITIES];
int         discEntityCount  = 0;
bool        discDone         = false;
bool        dwDataValid      = false;
unsigned long lastDwFetchMillis  = 0;
unsigned long lastDiscoverMillis = 0;

void translitAscii(const char* src, char* dst, size_t dstSize) {
  size_t j = 0;
  for (size_t i = 0; src[i] && j + 1 < dstSize; ) {
    unsigned char c = (unsigned char)src[i];
    if (c == 0xC3 && src[i+1]) {
      unsigned char c2 = (unsigned char)src[i+1];
      const char* rep = nullptr;
      switch(c2) {
        case 0xA4: rep="ae"; break; case 0x84: rep="Ae"; break;
        case 0xB6: rep="oe"; break; case 0x96: rep="Oe"; break;
        case 0xBC: rep="ue"; break; case 0x9C: rep="Ue"; break;
        case 0x9F: rep="ss"; break;
      }
      if (rep) { while (*rep && j+1<dstSize) dst[j++] = *rep++; }
      i += 2;
    } else if (c < 0x80) {
      dst[j++] = (char)c; i++;
    } else {
      i++;
    }
  }
  dst[j] = '\0';
}

// Discover binary_sensors via HA label_entities (same approach as M5Stack-Dial).
// One template request returns all groups' sensors separated by '~'.
bool haDiscoverLabels(const String& baseUrl, const String& token) {
  String tmpl = "{\"template\":\"";
  bool any = false;
  for (int g = 0; g < MAX_LABEL_GROUPS; g++) {
    if (!labelGroups[g].used || !labelGroups[g].enabled) continue;
    any = true;
    String lbl = labelGroups[g].haLabel;
    // Include both directly-labelled entities and device-labelled entities
    tmpl += "{% for e in (label_entities('" + lbl + "') + (label_devices('" + lbl +
            "')|map('device_entities')|sum(start=[]))) | unique %}";
    // Exclude non-opening sensor classes
    tmpl += "{% if e.startswith('binary_sensor.') and state_attr(e,'device_class') not in "
            "['battery','tamper','problem','connectivity','update','running','moving','power','gas','smoke','moisture','sound','vibration'] %}";
    tmpl += String(g) + "|{{e}}|{{state_attr(e,'friendly_name')}}~";
    tmpl += "{% endif %}{% endfor %}";
  }
  tmpl += "\"}";

  if (!any) { discEntityCount = 0; discDone = true; return true; }

  HTTPClient http;
  if (!http.begin(baseUrl + "/api/template")) return false;
  http.addHeader("Authorization", "Bearer " + token);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(tmpl);
  if (code != 200) {
    Serial.printf("[HA] Discovery -> HTTP %d\n", code);
    http.end(); return false;
  }
  String payload = http.getString();
  http.end();

  int n = 0, start = 0;
  while (start < (int)payload.length() && n < MAX_DISC_ENTITIES) {
    int rec = payload.indexOf('~', start);
    if (rec < 0) break;
    String record = payload.substring(start, rec);
    start = rec + 1;
    int b1 = record.indexOf('|');
    int b2 = record.indexOf('|', b1+1);
    if (b1 < 0 || b2 < 0) continue;
    int grp = record.substring(0, b1).toInt();
    String eid = record.substring(b1+1, b2); eid.trim();
    String fn  = record.substring(b2+1);     fn.trim();
    if (!eid.startsWith("binary_sensor.")) continue;

    bool dup = false;
    for (int k=0; k<n; k++) if (strcmp(discEntities[k].entityId, eid.c_str()) == 0) { dup=true; break; }
    if (dup) continue;

    discEntities[n].group = (uint8_t)grp;
    strlcpy(discEntities[n].entityId, eid.c_str(), sizeof(discEntities[n].entityId));
    translitAscii(fn.c_str(), discEntities[n].name, sizeof(discEntities[n].name));
    discEntities[n].valid = false;
    discEntities[n].open  = false;
    n++;
  }
  discEntityCount = n;
  discDone = true;
  Serial.printf("[HA] Discovery: %d Sensoren gefunden\n", n);
  return true;
}

// Fetch on/off states for all discovered entities (one template request).
bool haFetchDwStates(const String& baseUrl, const String& token) {
  if (discEntityCount == 0) return false;

  String tmpl = "{\"template\":\"";
  for (int i=0; i<discEntityCount; i++) {
    if (i) tmpl += ",";
    tmpl += "{{states('"; tmpl += discEntities[i].entityId; tmpl += "')}}";
  }
  tmpl += "\"}";

  HTTPClient http;
  if (!http.begin(baseUrl + "/api/template")) return false;
  http.addHeader("Authorization", "Bearer " + token);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(tmpl);
  if (code != 200) { http.end(); return false; }
  String payload = http.getString();
  http.end();

  bool anyOk = false;
  int start = 0;
  for (int i=0; i<discEntityCount; i++) {
    int comma = payload.indexOf(',', start);
    String tok = (comma < 0) ? payload.substring(start) : payload.substring(start, comma);
    tok.trim();
    if (tok == "on" || tok == "off") {
      discEntities[i].open  = (tok == "on");
      discEntities[i].valid = true;
      anyOk = true;
    } else {
      discEntities[i].valid = false;
    }
    if (comma < 0) break;
    start = comma + 1;
  }
  return anyOk;
}

int countOpen() {
  int n = 0;
  for (int i=0; i<discEntityCount; i++) if (discEntities[i].valid && discEntities[i].open) n++;
  return n;
}

int countOpenInGroup(int group) {
  int n = 0;
  for (int i=0; i<discEntityCount; i++)
    if (discEntities[i].group == group && discEntities[i].valid && discEntities[i].open) n++;
  return n;
}

int countTotalInGroup(int group) {
  int n = 0;
  for (int i=0; i<discEntityCount; i++) if (discEntities[i].group == group) n++;
  return n;
}

#pragma once
#include <Arduino.h>
#include "config.h"

// ── Data structures ───────────────────────────────────────────────────────────

struct LabelGroup {
  char displayName[20];
  char haLabel[28];
  uint8_t iconType;  // LG_ICON_*
  bool used;
  bool enabled;
};

struct DiscEntity {
  uint8_t group;
  char    entityId[72];
  char    name[28];
  bool    open;
  bool    valid;
};

struct NameOverride {
  char entityId[72];
  char displayName[28];
};

// ── Globals (defined in ha.cpp) ───────────────────────────────────────────────
extern LabelGroup    labelGroups[MAX_LABEL_GROUPS];
extern DiscEntity    discEntities[MAX_DISC_ENTITIES];
extern int           discEntityCount;
extern bool          discDone;
extern bool          dwDataValid;
extern unsigned long lastDwFetchMillis;
extern unsigned long lastDiscoverMillis;
extern NameOverride  nameOverrides[MAX_DISC_ENTITIES];
extern int           nameOverrideCount;

// ── API ───────────────────────────────────────────────────────────────────────

// Transliterate UTF-8 umlauts to ASCII for display (same as M5Stack-Dial)
void translitAscii(const char* src, char* dst, size_t dstSize);

// Discover door/window binary_sensors via HA label_entities() template.
// Returns true on success.
bool haDiscoverLabels(const String& baseUrl, const String& token);

// Fetch on/off states for all discovered entities (one template request).
bool haFetchDwStates(const String& baseUrl, const String& token);

// Apply nameOverrides to discEntities[].name — call after haDiscoverLabels().
void applyNameOverrides();

// Count open sensors (total and per group)
int countOpen();
int countOpenInGroup(int group);
int countTotalInGroup(int group);

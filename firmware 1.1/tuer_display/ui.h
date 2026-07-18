#pragma once
#include <Arduino.h>

void showBoot(const char* msg);
void showWifiPortal(const char* apName, const char* ip);
void showConnecting(const char* ssid);
void showError(const char* msg);
void showWifiFail(const char* savedSsid);
void showSettings(const char* ssid, const char* ip, int rssi, bool webActive);
// Screen 0: Übersicht – nur offene Sensoren, nur WINDOW/DOOR-Gruppen, Separator tiefer
void showOverviewScreen(const String& timeStr, const String& ipStr, bool wifiOk, bool webActive);
// Screen 1+: Detailansicht einer Gruppe – alle Sensoren
void showGroupScreen(int groupIdx, const String& timeStr, const String& ipStr, bool wifiOk, bool webActive);

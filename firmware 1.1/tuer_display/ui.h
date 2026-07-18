#pragma once
#include <Arduino.h>

void showBoot(const char* msg);
void showWifiPortal(const char* apName, const char* ip);
void showConnecting(const char* ssid);
void showError(const char* msg);
void showWifiFail(const char* savedSsid);
void showSettings(const char* ssid, const char* ip, int rssi, bool webActive);
void showDoorWindowScreen(const String& timeStr, bool wifiOk, bool webActive);

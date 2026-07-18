#pragma once
#include <Arduino.h>

void showBoot(const char* msg);
void showWifiPortal(const char* apName, const char* ip);
void showConnecting(const char* ssid);
void showError(const char* msg);
void showDoorWindowScreen(const String& timeStr, bool wifiOk, bool webActive);

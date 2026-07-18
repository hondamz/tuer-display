#pragma once
#include <Adafruit_GFX.h>
#include <pgmspace.h>
#include "src/display/epaper_driver_bsp.h"

#define EPD_W   200
#define EPD_H   200
#define BLACK   DRIVER_COLOR_BLACK
#define WHITE   DRIVER_COLOR_WHITE
#define BPR     25   // bytes per row (200 / 8)

// Global display pointer – defined in tuer_display.ino
extern epaper_driver_display* display;

// Pixel primitives
void px(int x, int y, uint8_t c);
void fillRect(int x, int y, int w, int h, uint8_t c);
void hline(int x, int y, int w, uint8_t c);
void vline(int x, int y, int h, uint8_t c);
void strokeRect(int x, int y, int w, int h, int t, uint8_t c);
void fillCircle(int cx, int cy, int r, uint8_t c);
void strokeCircle(int cx, int cy, int r, int t, uint8_t c);
void line(int x0, int y0, int x1, int y1, uint8_t c);
void fillRoundRect(int x, int y, int w, int h, int r, uint8_t c);
void strokeRoundRect(int x, int y, int w, int h, int r, int t, uint8_t c);

// Screen control
void clearWhite();
void refreshFull();
void refreshPart();

// Text rendering (proportional GFX fonts)
const GFXfont* uiFontForScale(int scale);
int  uiFontHeight(int scale);
int  textW(const char* s, int scale);
void drawStr(int x, int y, const char* s, int scale, uint8_t color);
void drawStrC(int cx, int y, const char* s, int scale, uint8_t color);
void drawStrFit(int x, int y, int maxW, const char* s, int scale, uint8_t color);

// Umlaut normalization
String normStr(const String& in);

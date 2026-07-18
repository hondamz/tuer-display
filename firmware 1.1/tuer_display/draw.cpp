#include "Arduino.h"
#include "draw.h"
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <math.h>

// ── Pixel ─────────────────────────────────────────────────────────────────────

void px(int x, int y, uint8_t c) {
  if ((unsigned)x >= EPD_W || (unsigned)y >= EPD_H) return;
  uint8_t* buf = display->getBuffer();
  int      idx = y * BPR + (x >> 3);
  uint8_t  bit = 1 << (7 - (x & 7));
  if (c == BLACK) buf[idx] &= ~bit; else buf[idx] |= bit;
}

// ── Rectangles ────────────────────────────────────────────────────────────────

void fillRect(int x, int y, int w, int h, uint8_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x >= EPD_W || y >= EPD_H || w <= 0 || h <= 0) return;
  if (x + w > EPD_W) w = EPD_W - x;
  if (y + h > EPD_H) h = EPD_H - y;
  uint8_t* buf = display->getBuffer();
  int x2    = x + w - 1;
  int byteL = x  >> 3, byteR = x2 >> 3;
  uint8_t mL = 0xFF >> (x  & 7);
  uint8_t mR = 0xFF << (7 - (x2 & 7));
  for (int r = 0; r < h; r++) {
    uint8_t* row = buf + (y + r) * BPR;
    if (byteL == byteR) {
      uint8_t m = mL & mR;
      if (c == BLACK) row[byteL] &= ~m; else row[byteL] |= m;
    } else {
      if (c == BLACK) {
        row[byteL] &= ~mL;
        memset(row + byteL + 1, 0x00, byteR - byteL - 1);
        row[byteR] &= ~mR;
      } else {
        row[byteL] |= mL;
        memset(row + byteL + 1, 0xFF, byteR - byteL - 1);
        row[byteR] |= mR;
      }
    }
  }
}

void hline(int x, int y, int w, uint8_t c) { fillRect(x, y, w, 1, c); }

void vline(int x, int y, int h, uint8_t c) {
  if ((unsigned)x >= EPD_W) return;
  if (y < 0) { h += y; y = 0; }
  if (y + h > EPD_H) h = EPD_H - y;
  if (h <= 0) return;
  uint8_t* buf = display->getBuffer();
  int idx = y * BPR + (x >> 3);
  uint8_t bit = 1 << (7 - (x & 7));
  if (c == BLACK) { for (int i=0;i<h;i++,idx+=BPR) buf[idx]&=~bit; }
  else            { for (int i=0;i<h;i++,idx+=BPR) buf[idx]|= bit; }
}

void strokeRect(int x, int y, int w, int h, int t, uint8_t c) {
  for (int i=0;i<t;i++) {
    hline(x+i,y+i,w-2*i,c); hline(x+i,y+h-1-i,w-2*i,c);
    vline(x+i,y+i,h-2*i,c); vline(x+w-1-i,y+i,h-2*i,c);
  }
}

void fillCircle(int cx, int cy, int r, uint8_t c) {
  for (int dy=-r; dy<=r; dy++) {
    int dx = (int)sqrtf((float)(r*r - dy*dy));
    fillRect(cx-dx, cy+dy, 2*dx+1, 1, c);
  }
}

void strokeCircle(int cx, int cy, int r, int t, uint8_t c) {
  for (int i=0;i<t;i++) {
    int rr=r-i, x=rr, y=0, err=0;
    while (x>=y) {
      px(cx+x,cy+y,c);px(cx+y,cy+x,c);px(cx-y,cy+x,c);px(cx-x,cy+y,c);
      px(cx-x,cy-y,c);px(cx-y,cy-x,c);px(cx+y,cy-x,c);px(cx+x,cy-y,c);
      y++;err+=1+2*y;
      if(2*(err-x)+1>0){x--;err+=1-2*x;}
    }
  }
}

void line(int x0, int y0, int x1, int y1, uint8_t c) {
  int dx=abs(x1-x0),dy=abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx-dy;
  while(true){px(x0,y0,c);if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>-dy){err-=dy;x0+=sx;}if(e2<dx){err+=dx;y0+=sy;}}
}

void fillRoundRect(int x, int y, int w, int h, int r, uint8_t c) {
  if (r <= 0) { fillRect(x,y,w,h,c); return; }
  r = min(r, min(w,h)/2);
  fillRect(x+r,   y,   w-2*r, h,     c);
  fillRect(x,     y+r, r,     h-2*r, c);
  fillRect(x+w-r, y+r, r,     h-2*r, c);
  fillCircle(x+r,     y+r,     r, c);
  fillCircle(x+w-r-1, y+r,     r, c);
  fillCircle(x+r,     y+h-r-1, r, c);
  fillCircle(x+w-r-1, y+h-r-1, r, c);
}

void strokeRoundRect(int x, int y, int w, int h, int r, int t, uint8_t c) {
  fillRoundRect(x, y, w, h, r, c);
  uint8_t bg = (c==BLACK) ? WHITE : BLACK;
  if (w>2*t && h>2*t) fillRoundRect(x+t, y+t, w-2*t, h-2*t, max(r-t,0), bg);
}

// ── Screen control ────────────────────────────────────────────────────────────

void clearWhite()   { display->EPD_Clear(); }
void refreshFull()  { display->EPD_Init(); display->EPD_Display(); }
void refreshPart()  { display->EPD_DisplayPart(); }

// ── Fonts ─────────────────────────────────────────────────────────────────────

const GFXfont* uiFontForScale(int scale) {
  if (scale <= 1) return &FreeSans9pt7b;
  return &FreeSansBold12pt7b;
}

int uiFontHeight(int scale) {
  return (scale <= 1) ? 14 : 22;
}

static void textBoundsFont(const char* s, int scale, int* minX, int* minY,
                            int* maxX, int* maxY, int* advOut = nullptr) {
  const GFXfont* font = uiFontForScale(scale);
  uint8_t first = pgm_read_byte(&font->first);
  uint8_t last  = pgm_read_byte(&font->last);
  int x = 0;
  *minX =  32767; *minY =  32767;
  *maxX = -32768; *maxY = -32768;
  while (*s) {
    uint8_t c = (uint8_t)*s++;
    if (c >= first && c <= last) {
      GFXglyph* glyph = &(((GFXglyph*)pgm_read_ptr(&font->glyph))[c - first]);
      uint8_t gw = pgm_read_byte(&glyph->width);
      uint8_t gh = pgm_read_byte(&glyph->height);
      int8_t  xo = pgm_read_byte(&glyph->xOffset);
      int8_t  yo = pgm_read_byte(&glyph->yOffset);
      uint8_t xa = pgm_read_byte(&glyph->xAdvance);
      if (gw && gh) {
        int gx1 = x+xo, gy1 = yo, gx2 = gx1+gw, gy2 = gy1+gh;
        if (gx1<*minX)*minX=gx1; if (gy1<*minY)*minY=gy1;
        if (gx2>*maxX)*maxX=gx2; if (gy2>*maxY)*maxY=gy2;
      }
      x += xa;
    }
  }
  if (*minX == 32767) { *minX=0;*minY=0;*maxX=0;*maxY=0; }
  if (advOut) *advOut = x;
}

int textW(const char* s, int scale) {
  int x1,y1,x2,y2,adv;
  textBoundsFont(s, scale, &x1, &y1, &x2, &y2, &adv);
  return max(x2-x1, adv);
}

static void drawGlyph(int x, int baseline, char ch, const GFXfont* font, uint8_t color) {
  uint8_t first = pgm_read_byte(&font->first);
  uint8_t last  = pgm_read_byte(&font->last);
  if ((uint8_t)ch < first || (uint8_t)ch > last) return;
  GFXglyph* glyph = &(((GFXglyph*)pgm_read_ptr(&font->glyph))[(uint8_t)ch - first]);
  uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
  uint8_t  w  = pgm_read_byte(&glyph->width);
  uint8_t  h  = pgm_read_byte(&glyph->height);
  int8_t   xo = pgm_read_byte(&glyph->xOffset);
  int8_t   yo = pgm_read_byte(&glyph->yOffset);
  uint8_t* bitmap = (uint8_t*)pgm_read_ptr(&font->bitmap);
  uint8_t bits = 0, bit = 0;
  for (int yy=0; yy<h; yy++)
    for (int xx=0; xx<w; xx++) {
      if (!(bit++ & 7)) bits = pgm_read_byte(&bitmap[bo++]);
      if (bits & 0x80) px(x+xo+xx, baseline+yo+yy, color);
      bits <<= 1;
    }
}

void drawStr(int x, int y, const char* s, int scale, uint8_t color) {
  const GFXfont* font = uiFontForScale(scale);
  int x1,y1,x2,y2,adv;
  textBoundsFont(s, scale, &x1, &y1, &x2, &y2, &adv);
  int cursor   = x - x1;
  int baseline = y - y1;
  while (*s) {
    uint8_t c = (uint8_t)*s++;
    if (c >= pgm_read_byte(&font->first) && c <= pgm_read_byte(&font->last)) {
      GFXglyph* g = &(((GFXglyph*)pgm_read_ptr(&font->glyph))[c - pgm_read_byte(&font->first)]);
      drawGlyph(cursor, baseline, (char)c, font, color);
      cursor += pgm_read_byte(&g->xAdvance);
    }
  }
}

void drawStrC(int cx, int y, const char* s, int scale, uint8_t color) {
  drawStr(cx - textW(s, scale) / 2, y, s, scale, color);
}

void drawStrFit(int x, int y, int maxW, const char* s, int scale, uint8_t color) {
  char buf[64];
  strncpy(buf, s ? s : "", sizeof(buf)-1);
  buf[sizeof(buf)-1] = 0;
  if (textW(buf, scale) <= maxW) { drawStr(x, y, buf, scale, color); return; }
  int len = strlen(buf);
  while (len > 0 && textW(buf, scale) > maxW) {
    len--;
    buf[len] = 0;
    if (len > 3) { buf[len-3]='.'; buf[len-2]='.'; buf[len-1]='.'; buf[len]=0; }
  }
  drawStr(x, y, buf, scale, color);
}

// ── String normalization (Umlaute → ASCII) ────────────────────────────────────

String normStr(const String& in) {
  String s = in;
  s.replace("ä","ae"); s.replace("ö","oe"); s.replace("ü","ue");
  s.replace("Ä","Ae"); s.replace("Ö","Oe"); s.replace("Ü","Ue");
  s.replace("ß","ss");
  String out = "";
  for (int i=0; i<(int)s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 32 && c <= 126) out += (char)c;
  }
  return out;
}

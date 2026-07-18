#pragma once

// ── E-Paper Display ──────────────────────────────────────────────────────────
#define EPD_WIDTH   200
#define EPD_HEIGHT  200
#define EPD_SPI_NUM SPI2_HOST

#define EPD_DC_PIN   GPIO_NUM_10
#define EPD_CS_PIN   GPIO_NUM_11
#define EPD_SCK_PIN  GPIO_NUM_12
#define EPD_MOSI_PIN GPIO_NUM_13
#define EPD_RST_PIN  GPIO_NUM_9
#define EPD_BUSY_PIN GPIO_NUM_8

// ── Power ─────────────────────────────────────────────────────────────────────
#define EPD_PWR_PIN   GPIO_NUM_6
#define AUDIO_PWR_PIN GPIO_NUM_42  // not used, BSP needs it
#define VBAT_PWR_PIN  GPIO_NUM_17

// ── Buttons ───────────────────────────────────────────────────────────────────
#define BTN_REC 0   // GPIO 0  – manual refresh
#define BTN_PWR 18  // GPIO 18 – toggle web config portal

// ── WiFi Manager AP ──────────────────────────────────────────────────────────
#define WIFI_AP_NAME     "TuerDisplay-Setup"
#define WIFI_AP_PASSWORD ""                   // open AP (no password)
#define WIFI_TIMEOUT_S   120                  // seconds to wait for portal

// ── Home Assistant ───────────────────────────────────────────────────────────
#define HA_DEFAULT_URL      "http://192.168.50.35:8123"
#define HA_DEFAULT_INTERVAL 30   // seconds between state polls
#define DISCOVER_INTERVAL_MS 600000UL  // 10 min between label re-discovery

// ── Display update ───────────────────────────────────────────────────────────
#define FULL_REFRESH_INTERVAL_MS 3600000UL  // full EPD refresh every 60 min (avoids ghosting)

// ── NVS namespace ─────────────────────────────────────────────────────────────
#define NVS_NS "tuerdisplay"

// ── Label groups ─────────────────────────────────────────────────────────────
#define MAX_LABEL_GROUPS  6
#define MAX_DISC_ENTITIES 40

enum { LG_ICON_WINDOW = 0, LG_ICON_DOOR = 1, LG_ICON_GENERIC = 2 };

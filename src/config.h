/*
 * config.h - Central configuration for the CNC touchscreen pendant.
 *
 * Board : Waveshare ESP32-S3-Touch-LCD-7 (800x480 RGB, GT911 touch, CH422G IO
 *         expander, microSD on SPI, native USB + CH343 USB-UART bridge)
 * Target: Arduino Uno + CNC shield running GRBL 1.1
 *
 * Everything tunable lives here: pins, feature toggles, WiFi, web, SD, jog.
 */
#pragma once

#include <Arduino.h>

// ===========================================================================
//  FEATURE TOGGLES  (flip to 0 to compile a feature out entirely)
// ===========================================================================
#define ENABLE_WIFI          1   // STA/AP provisioning + web server + websocket
#define ENABLE_WEB           1   // HTTP UI + REST + WebSocket mirror (needs WIFI)
#define ENABLE_STORAGE       1   // job storage (LittleFS flash + SD if present)
#define ENABLE_SD            1   // microSD over SPI (CS on CH422G EXIO4)
#define ENABLE_USB_FEED      1   // accept G-code from a PC over native USB CDC

#if ENABLE_WEB && !ENABLE_WIFI
#error "ENABLE_WEB requires ENABLE_WIFI"
#endif

// ===========================================================================
//  GRBL serial link  (ESP32-S3 UART1 -> level shifter -> Uno pins 0/1)
// ===========================================================================
// We talk to GRBL over UART1 on the P1 header pins GPIO43 (TX) / GPIO44 (RX).
// Set the onboard UART selection switch to the *header* side.
//
// IMPORTANT: The Uno is 5V, the ESP32-S3 is 3.3V and NOT 5V tolerant. Wire both
// directions through a bidirectional logic level converter:
//     ESP32 GPIO43 (TX) --LV--> shifter --HV--> Uno RX (pin 0)
//     Uno TX (pin 1)    --HV--> shifter --LV--> ESP32 GPIO44 (RX)
//     ESP32 3V3 -> shifter LV,  Uno 5V -> shifter HV,  GND common to both.
#define GRBL_UART            Serial1
#define GRBL_TX_PIN          43
#define GRBL_RX_PIN          44
#define GRBL_BAUD            115200

// GRBL 1.1 serial RX buffer size, used by the char-counting streaming protocol
// to keep the buffer as full as possible without overflowing.
#define GRBL_RX_BUFFER_SIZE  128

// How often to request a status report ("?") from GRBL, milliseconds.
#define GRBL_STATUS_POLL_MS  200

// ===========================================================================
//  Wired PC G-code input  (native USB CDC)
// ===========================================================================
// The ESP32-S3's native USB port enumerates as a CDC serial device (a COM port
// on the PC). A G-code sender / plain line stream on that port is forwarded to
// GRBL through the same char-counting streamer used for stored jobs. This is
// independent of WiFi and always available.
//
// NOTE: this is the ESP32-S3 *native* USB port, NOT the CH343 "UART" USB-C port
// (that one is UART0 for flashing, and UART1/UART2 share a single UART so it
// cannot be used for the PC feed at the same time as the GRBL link). The board
// JSON sets ARDUINO_USB_MODE=1 + ARDUINO_USB_CDC_ON_BOOT=1, so `Serial` == the
// native USB CDC. Flashing still works via the CH343 "UART" port.
#define USB_FEED_PORT        Serial

// ===========================================================================
//  microSD  (SPI; CS is on the CH422G IO expander at EXIO4, per schematic)
// ===========================================================================
// VERIFIED AGAINST SCHEMATIC, BUT NEEDS ON-HARDWARE VERIFICATION.
// The SD card shares no other device on this SPI bus, so we assert its chip
// select (EXIO4, active-low) via the CH422G and hold it for the session. The
// SPI CS GPIO handed to the SD driver is "none" (-1) since the real CS is on
// the expander.
#define SD_SPI_MOSI          11
#define SD_SPI_SCK           12
#define SD_SPI_MISO          13
#define SD_CS_EXPANDER_PIN   4          // CH422G EXIO4 (NOT an ESP32 GPIO)
#define SD_SPI_FREQ_HZ       20000000   // 20 MHz; drop if a card is flaky

// ===========================================================================
//  Job storage
// ===========================================================================
#define JOBS_DIR             "/jobs"     // directory used on both SD and flash
#define STORE_SD_PREFIX      "/sd"       // virtual path prefix for SD files
#define STORE_FLASH_PREFIX   "/flash"    // virtual path prefix for LittleFS files
#define JOB_EXTENSIONS       ".nc,.gcode,.ngc,.tap,.txt"

// ===========================================================================
//  WiFi
// ===========================================================================
// The pendant persists STA credentials in NVS (Preferences). If none are saved
// or the join fails, it falls back to a SoftAP so you can always reach the web
// UI to provision. The on-display WiFi screen scans + connects too.
#define WIFI_HOSTNAME        "cnc-pendant"
#define WIFI_AP_SSID         "CNC-Pendant"
#define WIFI_AP_PASS         "cnc12345"   // >= 8 chars, or "" for an open AP
#define WIFI_STA_TIMEOUT_MS  12000        // how long to wait for a STA join
#define WIFI_NVS_NAMESPACE   "cncwifi"

// ===========================================================================
//  Web server
// ===========================================================================
#define WEB_HTTP_PORT        80
#define WEB_WS_PORT          81           // arduinoWebSockets server port
#define WEB_STATE_PUSH_MS    200          // how often to broadcast machine state

// ===========================================================================
//  Jog defaults
// ===========================================================================
#define JOG_FEED_DEFAULT     1000.0f      // mm/min
#define JOG_STEP_DEFAULT     1.0f         // mm
#define JOG_CONTINUOUS_DIST  1000.0f      // "continuous" jog distance (cancelled on release)

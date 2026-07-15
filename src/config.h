/*
 * config.h - Central configuration for the CNC touchscreen pendant.
 *
 * Board : Waveshare ESP32-S3-Touch-LCD-7 (800x480 RGB, GT911 touch)
 * Target: Arduino Uno + CNC shield running GRBL 1.1
 */
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
//  GRBL serial link
// ---------------------------------------------------------------------------
// We talk to GRBL over UART1, routed to the board's P1 UART header pins.
// On the ESP32-S3-Touch-LCD-7 those broken-out pins are GPIO43 (TX) / GPIO44
// (RX). Set the onboard UART selection switch to the *header* side so these
// pins go to P1 instead of the CH343 USB-UART bridge.
//
// IMPORTANT: The Uno's logic is 5V, the ESP32-S3 is 3.3V and NOT 5V tolerant.
// Wire both directions through a bidirectional logic level converter:
//     ESP32 GPIO43 (TX) --LV--> shifter --HV--> Uno RX (pin 0)
//     Uno TX (pin 1)    --HV--> shifter --LV--> ESP32 GPIO44 (RX)
//     ESP32 3V3 -> shifter LV,  Uno 5V -> shifter HV,  GND common to both.
#define GRBL_UART            Serial1
#define GRBL_TX_PIN          43
#define GRBL_RX_PIN          44
#define GRBL_BAUD            115200

// GRBL 1.1 serial RX buffer size. Used by the char-counting streaming
// protocol to keep the buffer as full as possible without overflowing.
#define GRBL_RX_BUFFER_SIZE  128

// How often to request a status report ("?") from GRBL, milliseconds.
#define GRBL_STATUS_POLL_MS  200

// ---------------------------------------------------------------------------
//  WiFi (for uploading G-code jobs from a browser)
// ---------------------------------------------------------------------------
// Default: the pendant hosts its own access point so no router is needed in
// the shop. Connect a laptop/phone to this SSID, then browse to 192.168.4.1.
#define WIFI_AP_MODE         1           // 1 = SoftAP (recommended), 0 = join existing WiFi
#define WIFI_AP_SSID         "CNC-Pendant"
#define WIFI_AP_PASS         "cnc12345"  // >= 8 chars, or "" for open network

// Used only when WIFI_AP_MODE == 0
#define WIFI_STA_SSID        "your-wifi"
#define WIFI_STA_PASS        "your-pass"

#define WEB_SERVER_PORT      80

// ---------------------------------------------------------------------------
//  Jog defaults
// ---------------------------------------------------------------------------
#define JOG_FEED_DEFAULT     1000.0f     // mm/min
#define JOG_STEP_DEFAULT     1.0f        // mm

// Directory in LittleFS where uploaded jobs live
#define JOBS_DIR             "/jobs"

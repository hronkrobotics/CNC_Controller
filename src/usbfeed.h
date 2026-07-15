/*
 * usbfeed.h - Wired PC G-code input over the ESP32-S3 native USB CDC port.
 *
 * A plain line stream (or a simple G-code sender) on the native USB port is
 * forwarded to GRBL through the same char-counting queue used for stored jobs,
 * giving flow control "for free". GRBL's replies (ok/error/status) are echoed
 * back to the PC so line-oriented senders get their acknowledgements.
 *
 * Real-time bytes (?, !, ~, Ctrl-X, 0x85) are passed straight through. This is
 * a pragmatic bridge, not a byte-perfect GRBL passthrough - documented as such.
 *
 * Runs on the GRBL/UI task (same task that owns GrblController), so it needs no
 * locking. Independent of WiFi.
 */
#pragma once

#include <Arduino.h>

void usb_feed_init();
void usb_feed_loop();                       // poll the USB CDC port
void usb_feed_out(const char *line, bool outgoing);   // echo a GRBL reply to PC

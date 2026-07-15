/*
 * wifimgr.h - WiFi provisioning + status.
 *
 * Boot behaviour: load STA credentials from NVS. If present, try to join; on
 * success we run in STA mode. If there are no credentials or the join fails,
 * fall back to a SoftAP so the web UI / on-display WiFi screen are always
 * reachable for provisioning. Credentials are saved to NVS on a successful
 * connect request. WiFi can be disabled entirely with ENABLE_WIFI == 0.
 *
 * All calls are non-blocking (scans and joins are polled) so they are safe to
 * drive from the LVGL task without freezing the UI.
 */
#pragma once

#include <Arduino.h>
#include <vector>

struct WifiScanResult {
    String ssid;
    int    rssi = 0;
    bool   secure = false;
};

void wifi_init();
void wifi_loop();                          // call periodically (state machine)

// Provisioning (non-blocking; watch wifi_up()/wifi_mode_str() for the result).
void wifi_connect(const String &ssid, const String &pass);
void wifi_forget();                        // clear saved creds, drop to AP
void wifi_start_ap();

// Async scan.
void wifi_scan_start();
int  wifi_scan_poll(std::vector<WifiScanResult> &out);   // -1 running, else count

// Status.
const char *wifi_mode_str();               // "AP" / "STA" / "OFF"
String      wifi_ip();
String      wifi_ssid();
int         wifi_rssi();
bool        wifi_up();                      // AP active or STA connected

#include "wifimgr.h"
#include "config.h"

#if ENABLE_WIFI
#include <WiFi.h>
#include <Preferences.h>

namespace {

enum class Mode { Off, Ap, StaJoining, Sta };

Mode        s_mode = Mode::Off;
Preferences s_prefs;
String      s_staSsid, s_staPass;
uint32_t    s_joinStart = 0;
bool        s_scanRunning = false;

void startAp()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, strlen(WIFI_AP_PASS) ? WIFI_AP_PASS : nullptr);
    s_mode = Mode::Ap;
}

void startJoin(const String &ssid, const String &pass)
{
    s_staSsid = ssid;
    s_staPass = pass;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.begin(ssid.c_str(), pass.c_str());
    s_joinStart = millis();
    s_mode = Mode::StaJoining;
}

} // namespace

void wifi_init()
{
    WiFi.persistent(false);
    s_prefs.begin(WIFI_NVS_NAMESPACE, false);
    String ssid = s_prefs.getString("ssid", "");
    String pass = s_prefs.getString("pass", "");

    if (ssid.length()) startJoin(ssid, pass);
    else               startAp();
}

void wifi_loop()
{
    if (s_mode == Mode::StaJoining) {
        if (WiFi.status() == WL_CONNECTED) {
            s_mode = Mode::Sta;
        } else if (millis() - s_joinStart > WIFI_STA_TIMEOUT_MS) {
            startAp();                          // join failed -> AP fallback
        }
    } else if (s_mode == Mode::Sta) {
        if (WiFi.status() != WL_CONNECTED) {
            startJoin(s_staSsid, s_staPass);    // try to reconnect
        }
    }
}

void wifi_connect(const String &ssid, const String &pass)
{
    s_prefs.putString("ssid", ssid);
    s_prefs.putString("pass", pass);
    startJoin(ssid, pass);
}

void wifi_forget()
{
    s_prefs.remove("ssid");
    s_prefs.remove("pass");
    startAp();
}

void wifi_start_ap() { startAp(); }

void wifi_scan_start()
{
    if (s_scanRunning) return;
    WiFi.scanDelete();
    WiFi.scanNetworks(true /* async */);
    s_scanRunning = true;
}

int wifi_scan_poll(std::vector<WifiScanResult> &out)
{
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return -1;
    if (n < 0) { s_scanRunning = false; return 0; }

    out.clear();
    for (int i = 0; i < n; i++) {
        WifiScanResult r;
        r.ssid   = WiFi.SSID(i);
        r.rssi   = WiFi.RSSI(i);
        r.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        if (r.ssid.length()) out.push_back(r);
    }
    WiFi.scanDelete();
    s_scanRunning = false;
    return (int)out.size();
}

const char *wifi_mode_str()
{
    switch (s_mode) {
        case Mode::Ap:         return "AP";
        case Mode::Sta:        return "STA";
        case Mode::StaJoining: return "STA";
        default:               return "OFF";
    }
}

String wifi_ip()
{
    if (s_mode == Mode::Ap)  return WiFi.softAPIP().toString();
    if (s_mode == Mode::Sta) return WiFi.localIP().toString();
    return String("");
}

String wifi_ssid()
{
    if (s_mode == Mode::Ap)  return String(WIFI_AP_SSID);
    return s_staSsid;
}

int  wifi_rssi() { return (s_mode == Mode::Sta) ? WiFi.RSSI() : 0; }

bool wifi_up()
{
    return (s_mode == Mode::Ap) || (s_mode == Mode::Sta && WiFi.status() == WL_CONNECTED);
}

#else  // ENABLE_WIFI == 0 -------------------------------------------------

void wifi_init() {}
void wifi_loop() {}
void wifi_connect(const String &, const String &) {}
void wifi_forget() {}
void wifi_start_ap() {}
void wifi_scan_start() {}
int  wifi_scan_poll(std::vector<WifiScanResult> &out) { out.clear(); return 0; }
const char *wifi_mode_str() { return "OFF"; }
String wifi_ip() { return String(""); }
String wifi_ssid() { return String(""); }
int  wifi_rssi() { return 0; }
bool wifi_up() { return false; }

#endif

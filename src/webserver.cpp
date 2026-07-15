#include "webserver.h"
#include "config.h"

#if ENABLE_WEB

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "machine.h"
#include "jobstore.h"
#include "wifimgr.h"
#include "web_index.h"

static WebServer        s_http(WEB_HTTP_PORT);
static WebSocketsServer s_ws(WEB_WS_PORT);
static File             s_upFile;
static String           s_upVpath;
static uint32_t         s_lastPush = 0;

// ---------------------------------------------------------------------------
//  WebSocket: inbound commands
// ---------------------------------------------------------------------------
static void handleWsText(uint8_t *payload, size_t len)
{
    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) return;
    const char *c = doc["c"] | "";
    if (!*c) return;

    MachineCommand m;
    if (!strcmp(c, "jog")) {
        m.cmd = MCmd::Jog;
        const char *ax = doc["axis"] | "X";
        m.arg = ax[0];
        m.f1  = doc["dist"] | 0.0f;
        m.f2  = doc["feed"] | JOG_FEED_DEFAULT;
    } else if (!strcmp(c, "jogcancel")) {
        m.cmd = MCmd::JogCancel;
    } else if (!strcmp(c, "zero")) {
        m.cmd = MCmd::Zero;
        const char *ax = doc["axis"] | "ALL";
        m.arg = (!strcmp(ax, "ALL")) ? 'A' : ax[0];
    } else if (!strcmp(c, "home")) {
        m.cmd = MCmd::Home;
        const char *w = doc["which"] | "ALL";
        m.arg = (!strcmp(w, "XY")) ? 'X' : (!strcmp(w, "Z")) ? 'Z' : 'A';
    } else if (!strcmp(c, "unlock")) {
        m.cmd = MCmd::Unlock;
    } else if (!strcmp(c, "hold")) {
        m.cmd = MCmd::Hold;
    } else if (!strcmp(c, "resume")) {
        m.cmd = MCmd::Resume;
    } else if (!strcmp(c, "stop")) {
        m.cmd = MCmd::Stop;
    } else if (!strcmp(c, "posmode")) {
        m.cmd = MCmd::PosMode;
        m.b1  = doc["work"] | true;
    } else if (!strcmp(c, "gcode")) {
        m.cmd = MCmd::Gcode;
        strncpy(m.text, doc["line"] | "", sizeof(m.text) - 1);
    } else if (!strcmp(c, "runjob")) {
        m.cmd = MCmd::RunJob;
        strncpy(m.text, doc["path"] | "", sizeof(m.text) - 1);
    } else if (!strcmp(c, "jobpause")) {
        m.cmd = MCmd::JobPause;
    } else if (!strcmp(c, "jobresume")) {
        m.cmd = MCmd::JobResume;
    } else if (!strcmp(c, "jobstop")) {
        m.cmd = MCmd::JobStop;
    } else {
        return;
    }
    machine_post_command(m);
}

static void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len)
{
    (void)num;
    if (type == WStype_TEXT) handleWsText(payload, len);
}

// ---------------------------------------------------------------------------
//  HTTP handlers
// ---------------------------------------------------------------------------
static void handleRoot()
{
    s_http.sendHeader("Cache-Control", "no-cache");
    s_http.send(200, "text/html", WEB_INDEX_HTML);
}

static void handleFiles()
{
    std::vector<JobEntry> files;
    store_list(files);

    JsonDocument doc;
    JsonArray arr = doc["files"].to<JsonArray>();
    for (auto &f : files) {
        JsonObject o = arr.add<JsonObject>();
        o["name"]  = f.name;
        o["size"]  = f.size;
        o["store"] = f.onSd ? "sd" : "flash";
        o["path"]  = f.vpath;
    }
    String out;
    serializeJson(doc, out);
    s_http.send(200, "application/json", out);
}

static void handleUpload()
{
    HTTPUpload &up = s_http.upload();
    if (up.status == UPLOAD_FILE_START) {
        s_upFile = store_open_write(up.filename, s_upVpath);
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (s_upFile) s_upFile.write(up.buf, up.currentSize);
    } else if (up.status == UPLOAD_FILE_END) {
        if (s_upFile) s_upFile.close();
    }
}

static void handleUploadDone()
{
    bool ok = s_upVpath.length() > 0;
    s_http.send(200, "application/json",
                ok ? String("{\"ok\":true,\"path\":\"") + s_upVpath + "\"}"
                   : String("{\"ok\":false,\"err\":\"no storage\"}"));
    s_upVpath = "";
}

static void handleDelete()
{
    if (!s_http.hasArg("path")) {
        s_http.send(400, "application/json", "{\"ok\":false,\"err\":\"path required\"}");
        return;
    }
    bool ok = store_delete(s_http.arg("path"));
    s_http.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

// ---------------------------------------------------------------------------
//  State + log broadcast
// ---------------------------------------------------------------------------
static void broadcastState()
{
    MachineSnapshot s;
    machine_get(s);

    JsonDocument d;
    d["t"]     = "state";
    d["state"] = s.stateName;
    d["conn"]  = s.connected;
    JsonArray w = d["work"].to<JsonArray>();
    JsonArray m = d["mach"].to<JsonArray>();
    for (int i = 0; i < 3; i++) { w.add(s.work[i]); m.add(s.mach[i]); }
    d["feed"] = s.feed;

    JsonObject job = d["job"].to<JsonObject>();
    job["name"]   = s.jobName;
    job["active"] = s.jobActive;
    job["paused"] = s.jobPaused;
    job["done"]   = s.jobDone;
    job["total"]  = s.jobTotal;
    job["pct"]    = s.jobPct;

    JsonObject wf = d["wifi"].to<JsonObject>();
    wf["mode"] = s.wifiMode;
    wf["ip"]   = s.wifiIp;
    wf["ssid"] = s.wifiSsid;
    wf["rssi"] = s.wifiRssi;
    wf["up"]   = s.wifiUp;

    String out;
    serializeJson(d, out);
    s_ws.broadcastTXT(out);
}

static void broadcastLogs()
{
    MachineLogLine l;
    int budget = 20;                          // cap work per loop
    while (budget-- > 0 && machine_log_pop(l)) {
        JsonDocument d;
        d["t"] = "log";
        d["o"] = l.outgoing;
        d["s"] = l.s;
        String out;
        serializeJson(d, out);
        s_ws.broadcastTXT(out);
    }
}

// ---------------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------------
void web_init()
{
    s_http.on("/", HTTP_GET, handleRoot);
    s_http.on("/api/files", HTTP_GET, handleFiles);
    s_http.on("/api/upload", HTTP_POST, handleUploadDone, handleUpload);
    s_http.on("/api/delete", HTTP_GET, handleDelete);
    s_http.onNotFound([]() { s_http.send(404, "text/plain", "not found"); });
    s_http.begin();

    s_ws.begin();
    s_ws.onEvent(onWsEvent);
}

void web_loop()
{
    s_ws.loop();
    s_http.handleClient();

    uint32_t now = millis();
    if (now - s_lastPush >= WEB_STATE_PUSH_MS) {
        s_lastPush = now;
        broadcastState();
    }
    broadcastLogs();
}

#else

void web_init() {}
void web_loop() {}

#endif

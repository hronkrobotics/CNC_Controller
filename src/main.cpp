/*
 * CNC Touchscreen Pendant
 * Waveshare ESP32-S3-Touch-LCD-7  ->  Arduino Uno + CNC shield (GRBL 1.1)
 *
 * - LVGL touch UI (DRO, jog, zeroing, homing, hold/resume/reset, console)
 * - Streams G-code jobs stored in flash (LittleFS), uploaded over WiFi
 * - Talks to GRBL over UART1 on GPIO43/44 (via a 3.3V<->5V level shifter)
 *
 * JobStore and WebUI are implemented here (see note in ui.h).
 */
#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <FS.h>
#include <algorithm>

#include "lvgl_v8_port.h"
#include "config.h"
#include "grbl.h"
#include "ui.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// ===========================================================================
//  JobStore - G-code files in LittleFS
// ===========================================================================
namespace {
String baseName(const String &path)
{
    int slash = path.lastIndexOf('/');
    return (slash < 0) ? path : path.substring(slash + 1);
}
}  // namespace

namespace JobStore {

bool begin()
{
    if (!LittleFS.begin(true)) return false;   // format on first boot if needed
    if (!LittleFS.exists(JOBS_DIR)) LittleFS.mkdir(JOBS_DIR);
    return true;
}

std::vector<JobFile> list()
{
    std::vector<JobFile> out;
    File dir = LittleFS.open(JOBS_DIR);
    if (!dir || !dir.isDirectory()) return out;

    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory())
            out.push_back({ baseName(String(f.name())), (uint32_t)f.size() });
        f = dir.openNextFile();
    }
    std::sort(out.begin(), out.end(),
              [](const JobFile &a, const JobFile &b) { return a.name < b.name; });
    return out;
}

String fullPath(const String &name) { return String(JOBS_DIR) + "/" + baseName(name); }
bool remove(const String &name)     { return LittleFS.remove(fullPath(name)); }
uint64_t totalBytes()               { return LittleFS.totalBytes(); }
uint64_t usedBytes()                { return LittleFS.usedBytes(); }

}  // namespace JobStore

// ===========================================================================
//  WebUI - WiFi + HTTP uploader
// ===========================================================================
namespace {

WebServer     g_server(WEB_SERVER_PORT);
String        g_ip;
String        g_ssid;
File          g_uploadFile;
volatile bool g_uploading = false;

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CNC Pendant - Jobs</title>
<style>
 body{font-family:system-ui,sans-serif;background:#12151c;color:#e6e9ef;margin:0;padding:16px}
 h1{font-size:20px;margin:0 0 12px}
 .card{background:#1b1f2a;border:1px solid #2a3140;border-radius:12px;padding:16px;margin-bottom:16px}
 .drop{border:2px dashed #3a465c;border-radius:12px;padding:28px;text-align:center;color:#9aa4b6}
 .drop.over{border-color:#4c8bf5;color:#cdd6e6}
 input[type=file]{display:none}
 .btn{background:#2f6bff;color:#fff;border:0;border-radius:8px;padding:9px 14px;font-size:14px;cursor:pointer}
 .btn.sm{padding:5px 10px;font-size:13px}
 .btn.del{background:#c0392b}
 table{width:100%;border-collapse:collapse}
 td,th{text-align:left;padding:8px 6px;border-bottom:1px solid #262d3a;font-size:14px}
 .bar{height:8px;background:#262d3a;border-radius:6px;overflow:hidden;margin-top:10px;display:none}
 .bar>div{height:100%;width:0;background:#2f6bff;transition:width .1s}
 .muted{color:#8a93a6;font-size:13px}
</style></head><body>
<h1>CNC Pendant &mdash; G-code Jobs</h1>
<div class="card">
  <div id="drop" class="drop">Drop .nc / .gcode / .txt files here, or
    <label class="btn sm" style="display:inline-block">browse<input id="file" type="file" multiple></label>
  </div>
  <div class="bar" id="bar"><div id="fill"></div></div>
</div>
<div class="card">
  <table id="tbl"><thead><tr><th>File</th><th>Size</th><th></th></tr></thead><tbody></tbody></table>
  <p class="muted" id="usage"></p>
</div>
<script>
const $=s=>document.querySelector(s);
function fmt(n){return n<1024?n+' B':n<1048576?(n/1024).toFixed(1)+' KB':(n/1048576).toFixed(2)+' MB';}
async function refresh(){
  const r=await fetch('/list'); const j=await r.json();
  const tb=$('#tbl tbody'); tb.innerHTML='';
  j.files.forEach(f=>{
    const tr=document.createElement('tr');
    tr.innerHTML=`<td>${f.name}</td><td>${fmt(f.size)}</td>`+
      `<td><button class="btn sm del" onclick="del('${f.name}')">Delete</button></td>`;
    tb.appendChild(tr);
  });
  $('#usage').textContent=`Storage: ${fmt(j.used)} used of ${fmt(j.total)}`;
}
async function del(n){ if(!confirm('Delete '+n+'?'))return; await fetch('/delete?name='+encodeURIComponent(n)); refresh(); }
function uploadFiles(files){
  if(!files.length)return;
  const fd=new FormData();
  for(const f of files) fd.append('file',f,f.name);
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/upload');
  $('#bar').style.display='block';
  xhr.upload.onprogress=e=>{ if(e.lengthComputable) $('#fill').style.width=(100*e.loaded/e.total)+'%'; };
  xhr.onload=()=>{ $('#bar').style.display='none'; $('#fill').style.width='0'; refresh(); };
  xhr.send(fd);
}
const d=$('#drop');
['dragenter','dragover'].forEach(e=>d.addEventListener(e,ev=>{ev.preventDefault();d.classList.add('over');}));
['dragleave','drop'].forEach(e=>d.addEventListener(e,ev=>{ev.preventDefault();d.classList.remove('over');}));
d.addEventListener('drop',ev=>uploadFiles(ev.dataTransfer.files));
$('#file').addEventListener('change',ev=>uploadFiles(ev.target.files));
refresh();
</script></body></html>)HTML";

String sanitizeName(const String &fn)
{
    int slash = fn.lastIndexOf('/');
    String b = (slash < 0) ? fn : fn.substring(slash + 1);
    b.replace('\\', '_');
    return b;
}

void handleUpload()
{
    HTTPUpload &up = g_server.upload();
    if (up.status == UPLOAD_FILE_START) {
        g_uploading = true;
        String path = JobStore::fullPath(sanitizeName(up.filename));
        if (LittleFS.exists(path)) LittleFS.remove(path);
        g_uploadFile = LittleFS.open(path, "w");
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (g_uploadFile) g_uploadFile.write(up.buf, up.currentSize);
    } else {  // END or ABORTED
        if (g_uploadFile) g_uploadFile.close();
        g_uploading = false;
    }
}

void handleList()
{
    String json = "{\"files\":[";
    auto files = JobStore::list();
    for (size_t i = 0; i < files.size(); i++) {
        if (i) json += ',';
        json += "{\"name\":\"" + files[i].name + "\",\"size\":" + String(files[i].size) + "}";
    }
    json += "],\"used\":" + String((uint32_t)JobStore::usedBytes());
    json += ",\"total\":" + String((uint32_t)JobStore::totalBytes()) + "}";
    g_server.send(200, "application/json", json);
}

void handleDelete()
{
    if (g_server.hasArg("name")) JobStore::remove(g_server.arg("name"));
    g_server.send(200, "text/plain", "ok");
}

}  // namespace

namespace WebUI {

void begin()
{
#if WIFI_AP_MODE
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, strlen(WIFI_AP_PASS) ? WIFI_AP_PASS : nullptr);
    g_ssid = WIFI_AP_SSID;
    g_ip   = WiFi.softAPIP().toString();
#else
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(100);
    g_ssid = WIFI_STA_SSID;
    g_ip   = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("no-wifi");
#endif

    g_server.on("/", HTTP_GET, []() { g_server.send_P(200, "text/html", INDEX_HTML); });
    g_server.on("/list", HTTP_GET, handleList);
    g_server.on("/delete", HTTP_GET, handleDelete);
    g_server.on("/upload", HTTP_POST,
                []() { g_server.send(200, "text/plain", "ok"); },
                handleUpload);
    g_server.begin();
}

void   handle()      { g_server.handleClient(); }
String ipAddress()   { return g_ip; }
String ssid()        { return g_ssid; }
bool   isUploading() { return g_uploading; }

}  // namespace WebUI

// ===========================================================================
//  Application glue
// ===========================================================================
static void grblTimerCb(lv_timer_t *) { Grbl.update(); }
static void uiTimerCb(lv_timer_t *)   { Ui::tick();    }

void setup()
{
    Serial.begin(115200);
    Serial.println("[CNC] boot");

    // --- Storage + network first, so the UI can show connection info ---
    if (!JobStore::begin()) Serial.println("[CNC] LittleFS mount failed");
    WebUI::begin();
    Serial.printf("[CNC] WiFi '%s' at %s\n", WebUI::ssid().c_str(), WebUI::ipAddress().c_str());

    // --- GRBL serial link ---
    Grbl.begin();
    Grbl.onLog([](const String &line, bool out) { Ui::logLine(line, out); });

    // --- Display + touch (handled by the supported-board profile) ---
    Serial.println("[CNC] init panel");
    Board *board = new Board();
    board->init();
    assert(board->begin());

    Serial.println("[CNC] init LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    // --- Build UI + start periodic tasks (LVGL APIs need the lock) ---
    lvgl_port_lock(-1);
    Ui::create();
    lv_timer_create(grblTimerCb, 10, nullptr);   // pump GRBL every 10 ms
    lv_timer_create(uiTimerCb, 150, nullptr);    // refresh UI ~7 Hz
    lvgl_port_unlock();

    Serial.println("[CNC] ready");
}

void loop()
{
    // WiFi/HTTP file uploads run on the Arduino task; the UI + GRBL run on the
    // LVGL task via the timers created above.
    WebUI::handle();
    delay(2);
}

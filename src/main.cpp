/*
 * CNC Touchscreen Control Pendant  +  Web Mirror
 * Waveshare ESP32-S3-Touch-LCD-7  ->  Arduino Uno + CNC shield (GRBL 1.1)
 *
 * Architecture / threading
 * ------------------------
 *  - The display/touch are brought up by ESP32_Display_Panel; LVGL is attached
 *    via the example's lvgl_v8_port glue and runs on ITS OWN task.
 *  - The "machine owner" task == the LVGL task. Inside it (via lv_timer) we:
 *      * service the GRBL UART link          (grbl_timer_cb)
 *      * drain the native-USB-CDC G-code feed (grbl_timer_cb)
 *      * execute commands posted by the web   (grbl_timer_cb -> exec_command)
 *      * refresh the UI + publish the shared snapshot (ui_timer_cb)
 *    So GrblController is only ever touched from ONE task -> no locking there.
 *  - WiFi + the web server (HTTP/REST/WebSocket) run on the Arduino loop()
 *    task. They talk to the machine ONLY through the thread-safe machine_*
 *    API (commands in, snapshot + log out). This keeps the machine state a
 *    single source of truth mirrored to both the display and the browser.
 */
#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"

#include "config.h"
#include "grbl.h"
#include "ui.h"
#include "machine.h"
#include "sdcard.h"
#include "jobstore.h"
#include "wifimgr.h"
#include "webserver.h"
#include "usbfeed.h"

using namespace esp_panel::board;

// ---------------------------------------------------------------------------
//  Command execution (runs on the LVGL/GRBL task)
// ---------------------------------------------------------------------------
static void exec_command(const MachineCommand &m)
{
    switch (m.cmd) {
        case MCmd::Jog:       Grbl.jog(m.arg, m.f1, m.f2); break;
        case MCmd::JogCancel: Grbl.jogCancel();            break;
        case MCmd::Zero:
            if (m.arg == 'A') Grbl.zeroAll();
            else              Grbl.zeroAxis(m.arg);
            break;
        case MCmd::Home:
            if      (m.arg == 'X') { Grbl.sendLine("$HX"); Grbl.sendLine("$HY"); }
            else if (m.arg == 'Z') { Grbl.sendLine("$HZ"); }
            else                     Grbl.home();
            break;
        case MCmd::Unlock:    Grbl.unlock();    break;
        case MCmd::Hold:      Grbl.feedHold();  break;
        case MCmd::Resume:    Grbl.resume();    break;
        case MCmd::Stop:      Grbl.softReset(); break;
        case MCmd::PosMode:   ui_set_pos_mode(m.b1); break;
        case MCmd::Gcode:     Grbl.sendLine(m.text); break;
        case MCmd::RunJob: {
            fs::FS *fs = nullptr;
            String real;
            if (store_resolve(m.text, fs, real) && fs) Grbl.startJob(*fs, real);
            break;
        }
        case MCmd::JobPause:  Grbl.pauseJob();  break;
        case MCmd::JobResume: Grbl.resumeJob(); break;
        case MCmd::JobStop:   Grbl.stopJob();   break;
    }
}

// ---------------------------------------------------------------------------
//  Publish the shared snapshot (runs on the LVGL/GRBL task)
// ---------------------------------------------------------------------------
static void publish_snapshot()
{
    const GrblSnapshot &g = Grbl.status();
    MachineSnapshot s;

    strncpy(s.stateName, g.stateName, sizeof(s.stateName) - 1);
    s.state     = (int)g.state;
    s.connected = g.connected;
    for (int i = 0; i < 3; i++) { s.work[i] = g.wpos[i]; s.mach[i] = g.mpos[i]; }
    s.feed      = g.feed;
    s.showWork  = ui_get_pos_mode();

    s.jobActive = Grbl.jobActive();
    s.jobPaused = Grbl.jobPaused();
    s.jobDone   = Grbl.jobLinesDone();
    s.jobTotal  = Grbl.jobTotalLines();
    s.jobPct    = (int)(Grbl.jobProgress() * 100.0f);
    strncpy(s.jobName, Grbl.jobName().c_str(), sizeof(s.jobName) - 1);

    strncpy(s.wifiMode, wifi_mode_str(), sizeof(s.wifiMode) - 1);
    strncpy(s.wifiIp,   wifi_ip().c_str(),   sizeof(s.wifiIp) - 1);
    strncpy(s.wifiSsid, wifi_ssid().c_str(), sizeof(s.wifiSsid) - 1);
    s.wifiRssi  = wifi_rssi();
    s.wifiUp    = wifi_up();

    machine_publish(s);
}

// ---------------------------------------------------------------------------
//  Timers (LVGL task)
// ---------------------------------------------------------------------------
static void grbl_timer_cb(lv_timer_t *t)
{
    (void)t;
    Grbl.update();
    usb_feed_loop();
    MachineCommand m;
    while (machine_poll_command(m)) exec_command(m);
}

static void ui_timer_cb(lv_timer_t *t)
{
    (void)t;
    ui_refresh(Grbl.status());
    publish_snapshot();
}

// ---------------------------------------------------------------------------
//  Setup / loop
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    Serial.println("[CNC Pendant] booting");

    machine_init();

    Board *board = new Board();
    board->init();
    assert(board->begin());

    lvgl_port_init(board->getLCD(), board->getTouch());

    // microSD (CS on CH422G EXIO4). Failsafe: continue if it doesn't mount.
    esp_expander::Base *expander = nullptr;
    if (board->getIO_Expander()) expander = board->getIO_Expander()->getBase();
    bool sdOk = sd_init(expander);
    Serial.printf("[CNC Pendant] SD %s\n", sdOk ? "mounted" : "unavailable");

    store_init();

    Grbl.begin();
    Grbl.onLog([](const String &line, bool outgoing) {
        ui_console_log(line, outgoing);          // LVGL console
        machine_log_push(outgoing, line.c_str()); // web console
        usb_feed_out(line.c_str(), outgoing);     // echo to PC over USB CDC
    });

    usb_feed_init();

    lvgl_port_lock(-1);
    ui_init();
    lv_timer_create(grbl_timer_cb, 10, nullptr);    // service GRBL + USB + cmds
    lv_timer_create(ui_timer_cb, 120, nullptr);     // refresh UI + publish state
    lvgl_port_unlock();

    // Network last so the screen is already responsive.
    wifi_init();
    web_init();

    Serial.println("[CNC Pendant] ready");
}

void loop()
{
    wifi_loop();
    web_loop();
    delay(2);
}

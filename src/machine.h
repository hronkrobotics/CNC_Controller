/*
 * machine.h - Single source of truth shared by the LVGL display and the web UI.
 *
 * Threading model
 * ---------------
 * The GRBL link, command execution and the LVGL widget tree are all owned by
 * the LVGL task (see main.cpp lv_timer callbacks). The WiFi + web server run on
 * the Arduino loop() task. To keep GRBL access single-threaded and lock-free,
 * the two tasks communicate only through this module:
 *
 *   - Commands (web UI  -> machine): machine_post_command() from any task,
 *     drained by machine_poll_command() on the GRBL/LVGL task.
 *   - State    (machine -> web UI ): machine_publish() on the GRBL/LVGL task,
 *     read with machine_get() from the web task (mutex-protected snapshot).
 *   - Log lines (machine -> web UI): machine_log_push() from the GRBL/LVGL
 *     task, drained by machine_log_pop() on the web task.
 *
 * This way the actual GrblController is only ever touched from one task.
 */
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Shared machine snapshot (published by the GRBL/UI task, read by the web)
// ---------------------------------------------------------------------------
struct MachineSnapshot {
    char     stateName[12] = "---";
    int      state = 0;                 // GrblState as int
    bool     connected = false;         // UART link to GRBL alive
    float    work[3] = {0, 0, 0};
    float    mach[3] = {0, 0, 0};
    float    feed = 0;
    bool     showWork = true;           // display mode (work vs machine coords)

    // Current job
    char     jobName[40] = "";
    bool     jobActive = false;
    bool     jobPaused = false;
    uint32_t jobDone = 0;
    uint32_t jobTotal = 0;
    int      jobPct = 0;

    // WiFi
    char     wifiMode[4] = "OFF";       // "AP" / "STA" / "OFF"
    char     wifiIp[16] = "";
    char     wifiSsid[33] = "";
    int      wifiRssi = 0;
    bool     wifiUp = false;
};

// ---------------------------------------------------------------------------
//  Commands posted from the web UI to the machine
// ---------------------------------------------------------------------------
enum class MCmd : uint8_t {
    Jog, JogCancel, Zero, Home, Unlock, Hold, Resume, Stop,
    PosMode, Gcode, RunJob, JobPause, JobResume, JobStop
};

struct MachineCommand {
    MCmd  cmd = MCmd::JogCancel;
    char  arg = 0;          // Zero: 'X'/'Y'/'Z'/'A'(all);  Home: 'A'/'X'(xy)/'Z'
    float f1 = 0;           // Jog: distance (signed)
    float f2 = 0;           // Jog: feed
    bool  b1 = false;       // PosMode: show work?
    char  text[96] = "";    // Gcode line or RunJob path
};

// ---------------------------------------------------------------------------
//  Log lines forwarded to the web console
// ---------------------------------------------------------------------------
struct MachineLogLine {
    bool outgoing = false;
    char s[100] = "";
};

// ---------------------------------------------------------------------------
//  API
// ---------------------------------------------------------------------------
void machine_init();

// Commands (thread-safe)
bool machine_post_command(const MachineCommand &cmd);   // any task
bool machine_poll_command(MachineCommand &out);         // GRBL/UI task

// Snapshot (thread-safe)
void machine_publish(const MachineSnapshot &snap);      // GRBL/UI task
void machine_get(MachineSnapshot &out);                 // any task

// Log (thread-safe)
void machine_log_push(bool outgoing, const char *s);    // GRBL/UI task
bool machine_log_pop(MachineLogLine &out);              // web task

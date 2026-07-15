/*
 * ui.h - Public interfaces for the pendant UI plus the job-store and web
 *        uploader.
 *
 * NOTE: JobStore and WebUI are declared here (rather than in their own
 * headers) and implemented in main.cpp. All Ui:: functions must run inside the
 * LVGL task; main.cpp drives create()/tick() from LVGL timers.
 */
#pragma once

#include <Arduino.h>
#include <vector>
#include "config.h"

// --- G-code job files stored in flash (LittleFS) ---------------------------
struct JobFile {
    String   name;      // basename, e.g. "part.nc"
    uint32_t size;      // bytes
};

namespace JobStore {
    bool begin();                          // mount FS, ensure JOBS_DIR exists
    std::vector<JobFile> list();           // jobs sorted by name
    String fullPath(const String &name);   // JOBS_DIR + "/" + name
    bool remove(const String &name);
    uint64_t totalBytes();
    uint64_t usedBytes();
}

// --- WiFi + HTTP uploader --------------------------------------------------
namespace WebUI {
    void   begin();
    void   handle();                 // call from loop()
    String ipAddress();
    String ssid();
    bool   isUploading();            // true while a file transfer is in progress
}

// --- Touchscreen interface -------------------------------------------------
namespace Ui {
    void create();                                  // build the screen
    void tick();                                    // refresh from GRBL state
    void logLine(const String &line, bool outgoing);// append to the console
}

/*
 * jobstore.h - Unified G-code job storage over microSD (primary) + LittleFS
 * (flash fallback). Files live in JOBS_DIR on each medium and are addressed by
 * a virtual path: "/sd/<name>" or "/flash/<name>".
 */
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

struct JobEntry {
    String name;
    size_t size = 0;
    bool   onSd = false;
    String vpath;       // "/sd/foo.nc" or "/flash/foo.nc"
};

void store_init();

// True if the microSD is mounted (the preferred, larger medium).
bool store_sd_ready();

// List every job on both media.
void store_list(std::vector<JobEntry> &out);

// Resolve a virtual path to the backing filesystem + real on-disk path.
bool store_resolve(const String &vpath, fs::FS *&fs, String &realPath);

// Open a job for writing (uploads). Picks SD when available, else flash.
// Returns an open File and fills `vpathOut` with the virtual path.
File store_open_write(const String &name, String &vpathOut);

// Delete a job by virtual path.
bool store_delete(const String &vpath);

// Accepted file extension check (JOB_EXTENSIONS).
bool store_is_job_file(const String &name);

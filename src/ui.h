/*
 * ui.h - LVGL touchscreen UI for the CNC control pendant (800x480 landscape).
 *
 * The UI is created once with ui_init() and refreshed from the GRBL snapshot
 * with ui_refresh(). Console output is appended with ui_console_log(), and the
 * current job label is driven with ui_set_job(). The work/machine display mode
 * is shared with the web UI via ui_get/ set_pos_mode().
 *
 * All of these run inside the LVGL task (see main.cpp), so no extra locking is
 * required between them and the widget tree.
 */
#pragma once

#include "grbl.h"

void ui_init();
void ui_refresh(const GrblSnapshot &s);
void ui_console_log(const String &line, bool outgoing);
void ui_set_job(const String &name);

// Work vs machine coordinate display mode (shared with the web UI).
void ui_set_pos_mode(bool showWork);
bool ui_get_pos_mode();

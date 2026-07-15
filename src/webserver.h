/*
 * webserver.h - HTTP UI + REST API + WebSocket state mirror.
 *
 * Runs on the Arduino loop() task. Talks to the machine only through the
 * thread-safe machine_* API (commands in, snapshot + log out), so the GRBL
 * link stays owned by a single task. Compiled out when ENABLE_WEB == 0.
 */
#pragma once

void web_init();
void web_loop();

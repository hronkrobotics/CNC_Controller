/*
 * grbl.h - GRBL 1.1 controller interface.
 *
 * Owns the UART link to the Arduino/GRBL, parses "<...>" status reports,
 * sends real-time and line commands, and streams G-code jobs from LittleFS
 * using GRBL's character-counting flow control.
 *
 * All methods are intended to be called from a single task (the LVGL/UI task).
 */
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <functional>
#include <deque>
#include "config.h"

enum class GrblState {
    Unknown, Idle, Run, Hold, Jog, Alarm, Door, Home, Check, Sleep
};

struct GrblSnapshot {
    GrblState state = GrblState::Unknown;
    char      stateName[12] = "---";
    float     wpos[3] = {0, 0, 0};   // work position (what the operator cares about)
    float     mpos[3] = {0, 0, 0};   // machine position
    float     feed = 0;
    float     spindle = 0;
    int       ovFeed = 100, ovRapid = 100, ovSpindle = 100;
    bool      connected = false;     // true once we've parsed a status report
    uint32_t  lastReportMs = 0;
};

class GrblController {
public:
    using LogFn = std::function<void(const String &line, bool outgoing)>;

    void begin();
    void update();                       // non-blocking; call often

    // --- Real-time commands (sent immediately, bypass the queue) ---
    void feedHold();                     // '!'
    void resume();                       // '~'
    void softReset();                    // Ctrl-X (0x18)
    void jogCancel();                    // 0x85
    void requestStatus();                // '?'

    // --- Line commands (queued, char-counted) ---
    void sendLine(const String &line);
    void jog(char axis, float dist, float feed);
    void zeroAxis(char axis);            // set work zero for one axis (G10 L20 P1)
    void zeroAll();
    void home();                         // $H
    void unlock();                       // $X
    void gotoWorkXY0();                  // G90 G0 X0 Y0 (no Z move)

    // --- Job streaming ---
    bool     startJob(const String &path);
    void     stopJob();                  // abort: soft-reset + cleanup
    bool     jobActive() const { return _job.active; }
    float    jobProgress() const;        // 0..1
    uint32_t jobLinesDone() const { return _job.linesDone; }
    uint32_t jobTotalLines() const { return _job.totalLines; }
    const String &jobName() const { return _job.name; }

    const GrblSnapshot &status() const { return _snap; }
    void onLog(LogFn fn) { _log = fn; }

private:
    struct TxItem { String data; bool job; };
    struct Pending { uint16_t len; bool job; };

    void pumpTx();
    void handleByte(uint8_t b);
    void processLine(String line);       // a complete line received from GRBL
    void parseStatus(const String &s);
    void enqueue(const String &line, bool job);
    void feedJob();                      // read more lines from the file
    static String stripGcodeLine(const String &raw);

    GrblSnapshot _snap;
    LogFn        _log;
    uint32_t     _lastPoll = 0;

    String _rx;                          // incoming line assembly

    std::deque<TxItem>  _txQueue;        // waiting to be written
    std::deque<Pending> _pending;        // written, awaiting ok/error
    uint16_t            _pendingChars = 0;

    bool _wcoValid = false;
    float _wco[3] = {0, 0, 0};

    struct Job {
        bool     active = false;
        bool     reading = false;
        File     file;
        String   name;
        String   path;
        uint32_t totalLines = 0;
        uint32_t linesDone = 0;
    } _job;
};

extern GrblController Grbl;

#include "grbl.h"

GrblController Grbl;

// ---------------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------------
void GrblController::begin()
{
    GRBL_UART.begin(GRBL_BAUD, SERIAL_8N1, GRBL_RX_PIN, GRBL_TX_PIN);
    _rx.reserve(128);
}

void GrblController::update()
{
    // Drain incoming bytes.
    while (GRBL_UART.available()) {
        handleByte((uint8_t)GRBL_UART.read());
    }

    // Feed the job stream if we have room (paused jobs stop feeding new lines).
    if (_job.active && _job.reading && !_job.paused) {
        feedJob();
    }

    // Push queued lines out subject to char-counting flow control.
    pumpTx();

    // Detect job completion: nothing left to send or acknowledge.
    if (_job.active && !_job.reading && _txQueue.empty() && _pending.empty()) {
        if (_log) _log("[job finished: " + _job.name + "]", false);
        if (_job.file) _job.file.close();
        _job.active = false;
    }

    // Periodic status poll.
    uint32_t now = millis();
    if (now - _lastPoll >= GRBL_STATUS_POLL_MS) {
        _lastPoll = now;
        requestStatus();
    }

    // Mark disconnected if no report for a while.
    if (_snap.connected && (now - _snap.lastReportMs > 2000)) {
        _snap.connected = false;
    }
}

// ---------------------------------------------------------------------------
//  Real-time commands
// ---------------------------------------------------------------------------
void GrblController::feedHold()      { GRBL_UART.write((uint8_t)'!');  if (_log) _log("!", true); }
void GrblController::resume()        { GRBL_UART.write((uint8_t)'~');  if (_log) _log("~", true); }
void GrblController::jogCancel()     { GRBL_UART.write((uint8_t)0x85); }
void GrblController::requestStatus() { GRBL_UART.write((uint8_t)'?'); }

void GrblController::softReset()
{
    GRBL_UART.write((uint8_t)0x18);          // Ctrl-X
    if (_log) _log("[soft-reset]", true);
    // Everything buffered in GRBL is discarded on reset; clear our mirror.
    _txQueue.clear();
    _pending.clear();
    _pendingChars = 0;
    if (_job.active) {
        if (_job.file) _job.file.close();
        _job.active = false;
        _job.reading = false;
        _job.paused = false;
    }
}

// ---------------------------------------------------------------------------
//  Line commands
// ---------------------------------------------------------------------------
void GrblController::enqueue(const String &line, bool job)
{
    _txQueue.push_back({line, job});
}

void GrblController::sendLine(const String &line)
{
    String l = line;
    l.trim();
    if (l.length()) enqueue(l, false);
}

void GrblController::jog(char axis, float dist, float feed)
{
    // Incremental jog in mm. GRBL cancels jogs cleanly with 0x85.
    char buf[48];
    snprintf(buf, sizeof(buf), "$J=G91 G21 %c%.3f F%.0f", axis, dist, feed);
    enqueue(String(buf), false);
}

void GrblController::zeroAxis(char axis)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "G10 L20 P1 %c0", axis);
    enqueue(String(buf), false);
}

void GrblController::zeroAll()      { enqueue("G10 L20 P1 X0 Y0 Z0", false); }
void GrblController::home()         { enqueue("$H", false); }
void GrblController::unlock()       { enqueue("$X", false); }
void GrblController::gotoWorkXY0()  { enqueue("G90 G0 X0 Y0", false); }

// ---------------------------------------------------------------------------
//  TX flow control (character-counting protocol)
// ---------------------------------------------------------------------------
void GrblController::pumpTx()
{
    while (!_txQueue.empty()) {
        const TxItem &item = _txQueue.front();
        uint16_t need = item.data.length() + 1;   // +1 for '\n'
        if (_pendingChars + need > (GRBL_RX_BUFFER_SIZE - 1)) {
            break;                                  // would overflow GRBL's buffer
        }
        GRBL_UART.print(item.data);
        GRBL_UART.write((uint8_t)'\n');
        if (_log) _log(item.data, true);
        _pending.push_back({need, item.job});
        _pendingChars += need;
        _txQueue.pop_front();
    }
}

// ---------------------------------------------------------------------------
//  RX handling
// ---------------------------------------------------------------------------
void GrblController::handleByte(uint8_t b)
{
    if (b == '\n' || b == '\r') {
        if (_rx.length()) {
            processLine(_rx);
            _rx = "";
        }
        return;
    }
    _rx += (char)b;
    if (_rx.length() > 200) _rx = "";   // guard against runaway
}

void GrblController::processLine(String line)
{
    line.trim();
    if (!line.length()) return;

    if (line[0] == '<') {                       // status report
        parseStatus(line);
        return;
    }

    // Log everything the controller says (welcome text, [MSG:...], errors...).
    if (_log) _log(line, false);

    bool isOk    = line.equalsIgnoreCase("ok");
    bool isError = line.startsWith("error:");

    if (isOk || isError) {
        if (!_pending.empty()) {
            Pending p = _pending.front();
            _pending.pop_front();
            _pendingChars -= p.len;
            if (p.job) _job.linesDone++;
        }
        if (isError && _job.active) {
            // Stop the job on a G-code error so we don't keep cutting blindly.
            if (_log) _log("[job stopped on error]", false);
            softReset();
        }
    }
}

void GrblController::parseStatus(const String &s)
{
    // Format: <State|MPos:x,y,z|FS:f,s|WCO:x,y,z|Ov:100,100,100|...>
    int end = s.indexOf('>');
    String body = (end > 0) ? s.substring(1, end) : s.substring(1);

    float mpos[3], wpos[3];
    bool haveM = false, haveW = false;

    int start = 0;
    while (start < (int)body.length()) {
        int bar = body.indexOf('|', start);
        String field = (bar < 0) ? body.substring(start) : body.substring(start, bar);
        start = (bar < 0) ? body.length() : bar + 1;

        int colon = field.indexOf(':');
        if (colon < 0) {                        // first field: machine state
            field.toCharArray(_snap.stateName, sizeof(_snap.stateName));
            String st = field;
            int c = st.indexOf(':');            // e.g. "Hold:0" / "Door:1"
            if (c >= 0) st = st.substring(0, c);
            if      (st == "Idle")  _snap.state = GrblState::Idle;
            else if (st == "Run")   _snap.state = GrblState::Run;
            else if (st == "Hold")  _snap.state = GrblState::Hold;
            else if (st == "Jog")   _snap.state = GrblState::Jog;
            else if (st == "Alarm") _snap.state = GrblState::Alarm;
            else if (st == "Door")  _snap.state = GrblState::Door;
            else if (st == "Home")  _snap.state = GrblState::Home;
            else if (st == "Check") _snap.state = GrblState::Check;
            else if (st == "Sleep") _snap.state = GrblState::Sleep;
            else                    _snap.state = GrblState::Unknown;
            continue;
        }

        String key = field.substring(0, colon);
        String val = field.substring(colon + 1);

        if (key == "MPos" || key == "WPos") {
            float v[3] = {0, 0, 0};
            int i = 0, p = 0;
            while (i < 3 && p <= (int)val.length()) {
                int comma = val.indexOf(',', p);
                String num = (comma < 0) ? val.substring(p) : val.substring(p, comma);
                v[i++] = num.toFloat();
                if (comma < 0) break;
                p = comma + 1;
            }
            if (key == "MPos") { memcpy(mpos, v, sizeof(v)); haveM = true; }
            else               { memcpy(wpos, v, sizeof(v)); haveW = true; }
        } else if (key == "WCO") {
            float v[3] = {0, 0, 0};
            int i = 0, p = 0;
            while (i < 3 && p <= (int)val.length()) {
                int comma = val.indexOf(',', p);
                String num = (comma < 0) ? val.substring(p) : val.substring(p, comma);
                v[i++] = num.toFloat();
                if (comma < 0) break;
                p = comma + 1;
            }
            memcpy(_wco, v, sizeof(v));
            _wcoValid = true;
        } else if (key == "FS") {
            int comma = val.indexOf(',');
            _snap.feed    = val.substring(0, comma < 0 ? val.length() : comma).toFloat();
            _snap.spindle = comma < 0 ? 0 : val.substring(comma + 1).toFloat();
        } else if (key == "F") {
            _snap.feed = val.toFloat();
        } else if (key == "Ov") {
            int c1 = val.indexOf(',');
            int c2 = val.indexOf(',', c1 + 1);
            if (c1 > 0 && c2 > 0) {
                _snap.ovFeed    = val.substring(0, c1).toInt();
                _snap.ovRapid   = val.substring(c1 + 1, c2).toInt();
                _snap.ovSpindle = val.substring(c2 + 1).toInt();
            }
        }
    }

    // Resolve work vs machine coordinates. WPos = MPos - WCO.
    if (haveM) {
        memcpy(_snap.mpos, mpos, sizeof(mpos));
        if (_wcoValid)
            for (int i = 0; i < 3; i++) _snap.wpos[i] = mpos[i] - _wco[i];
        else
            memcpy(_snap.wpos, mpos, sizeof(mpos));
    }
    if (haveW) {
        memcpy(_snap.wpos, wpos, sizeof(wpos));
        if (_wcoValid)
            for (int i = 0; i < 3; i++) _snap.mpos[i] = wpos[i] + _wco[i];
    }

    _snap.connected = true;
    _snap.lastReportMs = millis();
}

// ---------------------------------------------------------------------------
//  Job streaming
// ---------------------------------------------------------------------------
String GrblController::stripGcodeLine(const String &raw)
{
    String out;
    out.reserve(raw.length());
    int inParen = 0;
    for (size_t i = 0; i < raw.length(); i++) {
        char c = raw[i];
        if (c == '(') { inParen++; continue; }         // block comments, nested
        if (c == ')') { if (inParen) inParen--; continue; }
        if (inParen) continue;
        if (c == ';') break;                           // line comment
        if (c == '\r' || c == '\n') continue;
        out += c;
    }
    out.trim();
    return out;
}

bool GrblController::startJob(fs::FS &fs, const String &path)
{
    if (_job.active) return false;

    File f = fs.open(path, "r");
    if (!f) {
        if (_log) _log("[cannot open " + path + "]", false);
        return false;
    }

    // Pre-scan to count the lines we will actually send (for progress).
    uint32_t total = 0;
    while (f.available()) {
        String l = f.readStringUntil('\n');
        if (stripGcodeLine(l).length()) total++;
    }
    f.close();

    _job.file = fs.open(path, "r");
    if (!_job.file) return false;

    _job.path       = path;
    _job.name       = path.substring(path.lastIndexOf('/') + 1);
    _job.totalLines = total;
    _job.linesDone  = 0;
    _job.reading    = true;
    _job.paused     = false;
    _job.active     = true;
    if (_log) _log("[job started: " + _job.name + " (" + String(total) + " lines)]", false);
    return true;
}

void GrblController::pauseJob()
{
    if (!_job.active || _job.paused) return;
    _job.paused = true;
    feedHold();                          // '!' - pause motion; buffer is retained
    if (_log) _log("[job paused]", false);
}

void GrblController::resumeJob()
{
    if (!_job.active || !_job.paused) return;
    _job.paused = false;
    resume();                            // '~' - resume motion
    if (_log) _log("[job resumed]", false);
}

void GrblController::feedJob()
{
    // Keep a modest backlog queued; pumpTx() enforces the real buffer limit.
    while (_txQueue.size() < 8 && _job.file && _job.file.available()) {
        String raw = _job.file.readStringUntil('\n');
        String g   = stripGcodeLine(raw);
        if (g.length()) {
            enqueue(g, true);
            return;                     // one line per call keeps things responsive
        }
    }
    if (_job.file && !_job.file.available()) {
        _job.reading = false;
    }
}

void GrblController::stopJob()
{
    if (!_job.active) return;
    softReset();                        // clears job state too
}

float GrblController::jobProgress() const
{
    if (_job.totalLines == 0) return 0.0f;
    float p = (float)_job.linesDone / (float)_job.totalLines;
    return p > 1.0f ? 1.0f : p;
}

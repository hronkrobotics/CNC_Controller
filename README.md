# CNC_controller

Design notes and firmware for a self-contained **CNC controller box** that drives a
GRBL-based router, with a 7" touchscreen pendant for jogging, DRO, and machine
control.

The repository currently holds the **ESP32-S3 touchscreen pendant firmware**
(PlatformIO). Hardware design details for the enclosure live here too so the whole
build is documented in one place.

---

## Hardware overview

| Subsystem | Details |
|---|---|
| **Motion controller** | Arduino Uno + CNC shield running **GRBL 1.1** |
| **Stepper drivers** | 4 × **TB6600** external drivers (one per axis) |
| **Motor power** | **36 V / 10 A** DC power supply |
| **Touchscreen / HMI** | **Waveshare ESP32-S3-Touch-LCD-7** (7", 800×480 IPS, GT911 capacitive touch, LVGL) |
| **Machine I/O** | 8 × **M12** panel connectors (motors / limit switches) on the enclosure |
| **Power distribution** | Mains → **rear-panel fuse + master switch**; **5 V buck converter** for logic/accessories; **Wago lever connectors** distribute the 5 V rail |
| **Cooling** | Fans on the 5 V rail |
| **Pendant ↔ GRBL link** | ESP32-S3 UART (3.3 V, `GPIO43`/`GPIO44`) ↔ Uno pins `0`/`1` (5 V) via a **bidirectional level shifter**, common ground |

### Serial link wiring

GRBL on the Uno uses pins **0 (RX)** / **1 (TX)** at 115200 baud. The ESP32-S3
talks on **UART1 → `GPIO43` (TX) / `GPIO44` (RX)**, broken out on the board's
**P1 header** (`GND · RX · TX · 3V3`). Set the board's **UART selection switch to
the header side** so those pins route to P1 instead of the onboard USB-serial.

The ESP32-S3 is **3.3 V and not 5 V-tolerant**; the Uno is 5 V. Use a
bidirectional logic level converter (BSS138 type):

```
 ESP32-S3 (P1 header)        Level shifter          Arduino Uno
   GPIO43 TX ───────── LV1 <────────> HV1 ─────────► RX (pin 0)
   GPIO44 RX ◄──────── LV2 <────────> HV2 ─────────  TX (pin 1)
   3V3       ───────── LV
                        HV ────────────────────────  5V
   GND       ───────── GND ─────────────────────────  GND   (common ground required)
```

> **Single-UART caveat:** the Uno's USB port and pins 0/1 are the *same* UART.
> A PC sender and the pendant cannot both be on the bus at once. Disconnect the
> pendant's TX/RX before re-flashing GRBL from a PC.

---

## Firmware (ESP32-S3 pendant)

A [PlatformIO](https://platformio.org/) project. The ESP32-S3 is the serial
master to GRBL, renders the touch UI, **and mirrors the whole machine to a web
UI over WiFi** — the machine state is a single source of truth shared by both.

### Architecture

```
   PC (G-code sender)                 Phone / laptop browser
        │ native USB CDC                     │ WiFi (HTTP + WebSocket)
        ▼                                    ▼
  ┌───────────────────────── ESP32-S3 pendant ─────────────────────────┐
  │  usbfeed ─┐                                    ┌─ webserver (loop task)│
  │           ▼                                    ▼   HTTP UI / REST / WS │
  │        ┌─────────── machine (single source of truth) ───────────┐     │
  │        │  cmd queue  ·  state snapshot  ·  log queue (thread-safe)│    │
  │        └───────────────────────▲──────────────────────────────┘      │
  │   LVGL UI ◄──┐                  │ publish / drain (LVGL task)          │
  │   (display)  └── grbl ◄─ streamer ◄─ jobstore ◄─ SD + LittleFS         │
  └───────┬─────────────────────────────────────────────────────────────┘
          │ UART1 GPIO43/44 → level shifter → Uno pins 0/1
          ▼
     Arduino Uno + CNC shield (GRBL 1.1)
```

**Threading:** GRBL, command execution and the LVGL widget tree are all owned by
the **LVGL task** (via `lv_timer`), so GRBL is only ever touched from one place.
WiFi + the web server run on the **`loop()` task**. The two communicate *only*
through the thread-safe `machine_*` API (commands in, state snapshot + console
log out). This keeps the display and browser perfectly in sync with no locks on
the GRBL core, and keeps network traffic off the timing-sensitive RGB refresh.

### Features

- Color-coded status chip (Idle / Run / Jog / Hold / Alarm / Disconnected)
- Large X/Y/Z **DRO** with a segmented **WORK/MACH** toggle (synced to the web)
- **Jog** cross (X/Y/Z ±) with `0.1 / 1 / 10 / 50 / 100` mm step + feed selector
  (units shown as *mm/min*), integrated **Home XY / Home Z**, jog-cancel
- **Zero** X / Y / Z / All, **Home** (`$H`), **Unlock** (`$X`)
- Always-visible **Feed-Hold / Resume / Soft-Reset (Stop)**
- Scrollable **console** of everything sent to / received from GRBL
- **JOBS** screen: list stored jobs (SD + flash), **Run** with a progress bar
  (line count + %), **Pause / Resume / Stop**
- **WiFi** screen: SSID scan + on-screen-keyboard password entry, saved to NVS;
  header shows WiFi mode + IP (tap it to open the WiFi screen)
- **Web mirror**: a browser at the pendant's IP shows the same DRO, status,
  console, jog/zero/home/unlock, hold/resume/stop and jobs — plus **G-code file
  upload**. Live state is pushed over a WebSocket.

### Three ways to run G-code

1. **Wired USB** — a PC on the ESP32-S3's **native USB port** (a COM port) streams
   G-code straight to GRBL through the char-counting queue. Always available,
   independent of WiFi. Real-time bytes (`?`, `!`, `~`, Ctrl-X, `0x85`) pass
   through; GRBL replies are echoed back to the PC.
2. **Stored job** — upload `.nc/.gcode/.ngc/.tap/.txt` over the web UI to SD (or
   flash), then **Run** it from the JOBS screen or the browser.
3. **Web stream** — send `gcode` commands from the browser console field.

### Build & flash

```bash
export PATH="$HOME/.local/bin:$PATH"
pio run                                          # build
pio run -t upload --upload-port /dev/ttyACM0     # build + flash
```

Flashing uses the ESP32-S3 **native USB port** (`/dev/ttyACM0` here). That same
port is the PC G-code feed at runtime. `CORE_DEBUG_LEVEL=0` keeps it clean for a
G-code sender; bump it in `platformio.ini` for verbose bring-up logs. If upload
can't sync, hold **BOOT**, tap **RESET**, release **BOOT**.

> The web UI is embedded in firmware (`src/web_index.h`, served from PROGMEM), so
> a single `pio run -t upload` ships everything — no separate filesystem upload.

The display, GT911 touch, backlight, and CH422G IO-expander are handled by
Espressif's `ESP32_Display_Panel` supported-board profile
`BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7`.

### WiFi / web

- First boot (no saved credentials) starts a **SoftAP** `CNC-Pendant` /
  `cnc12345`. Join it and browse to `http://192.168.4.1`, or use the on-display
  **WiFi** screen to scan + join your network (credentials persist in NVS).
- If a saved network can't be joined it falls back to the AP, so the UI is always
  reachable. Web = HTTP UI + REST on **:80**, WebSocket mirror on **:81**.
- Disable networking entirely with `ENABLE_WIFI 0` in `src/config.h`.

### microSD (CS on CH422G EXIO4)

Per the board schematic the SD card is on SPI (`MOSI=11 / SCK=12 / MISO=13`) with
its chip-select on the **CH422G IO expander at EXIO4**, not a GPIO. The firmware
grabs the expander from the initialized Board object
(`board->getIO_Expander()->getBase()`), asserts EXIO4 low to select the card, and
mounts it. **This is verified against the schematic but NEEDS ON-HARDWARE
VERIFICATION.** SD is fully optional: if it doesn't mount, the pendant runs
normally on flash (LittleFS) and the UI reports SD unavailable.

### Recommended GRBL settings (GRBL 1.1 defaults are fine)

- `$10=1` — status reports include machine position + work coordinate offset
- `$13=0` — report in millimetres (the UI assumes mm)
- Homing (`$22=1`) requires limit switches; on power-up GRBL may start in
  **Alarm** — tap **Unlock** (`$X`) or **Home** (`$H`)

### Project layout

| Path | Purpose |
|---|---|
| `platformio.ini` | build config, board, libraries |
| `boards/BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7.json` | 16 MB flash / 8 MB OPI PSRAM board def |
| `partitions.csv` | partition table |
| `src/config.h` | pins, baud, jog defaults, **feature toggles**, WiFi/web/SD — **edit me** |
| `src/grbl.*` | GRBL serial protocol, status parser, char-counting streamer |
| `src/machine.*` | single-source-of-truth state + cross-task command/log queues |
| `src/ui.*` | LVGL touchscreen interface (Control / Jobs / WiFi screens) |
| `src/usbfeed.*` | native-USB-CDC G-code input bridge |
| `src/sdcard.*` | microSD over SPI, CS on CH422G EXIO4 (failsafe) |
| `src/jobstore.*` | unified job store over SD + LittleFS (list/upload/delete) |
| `src/wifimgr.*` | STA + AP-fallback provisioning, NVS creds, async scan |
| `src/webserver.*` | HTTP UI + REST + WebSocket state mirror |
| `src/web_index.h` | embedded single-page web UI (served from PROGMEM) |
| `src/main.cpp` | board/LVGL init, task wiring, service timers |
| `src/lvgl_v8_port.*`, `src/lv_conf.h`, `src/esp_panel_*` | display / LVGL porting (from Espressif's example) |

### Feature toggles (`src/config.h`)

`ENABLE_WIFI`, `ENABLE_WEB`, `ENABLE_STORAGE`, `ENABLE_SD`, `ENABLE_USB_FEED` —
each compiles its subsystem in/out so you can strip the build down to the bare
pendant if desired.

### Fonts

The UI is unified on **Montserrat** at a range of sizes (every element routes
through a `FONT_*` define in `ui.cpp`) so there is no mixed-font patchwork. A
"techy" TTF (Share Tech Mono for labels, DSEG7 for the DRO) was *not* embedded
because `lv_font_conv` (node/npm) isn't available in this build environment —
there is a documented TODO in `ui.cpp` to generate those and repoint the
`FONT_*` defines.

---

## Safety

This is machine-control software with **no e-stop guarantees**. Keep a physical
emergency stop that cuts motor power independently of the screen. The **Stop**
button issues a GRBL soft-reset (Ctrl-X) — it halts motion but is not a
substitute for cutting power.

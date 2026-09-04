# Software

ESP32-S3 **pendant firmware** (PlatformIO). The motion controller is **GRBL 1.1 on the Arduino Uno**; this tree does not replace GRBL.

Hardware wiring: [hardware.md](hardware.md). Bring-up order: [instructions.md](../instructions.md). Tunables: [`src/config.h`](../src/config.h).

## Architecture

The ESP32-S3 is serial master to GRBL, renders the touch UI, and **mirrors the machine to a web UI over WiFi**. Machine state is one source of truth for both.

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

**Threading:** GRBL, command execution, and the LVGL widget tree are owned by the **LVGL task** (`lv_timer`). WiFi and the web server run on **`loop()`**. They talk only through the thread-safe `machine_*` API. That keeps the display and browser in sync, keeps locks off the GRBL core, and keeps network traffic off the RGB panel refresh.

## Features

- Status chip: Idle / Run / Jog / Hold / Alarm / Disconnected
- DRO with **WORK / MACH** toggle (synced to the web)
- Jog cross (X/Y/Z ±), steps `0.1 / 1 / 10 / 50 / 100` mm, feed in mm/min, Home XY / Home Z, jog-cancel
- Zero X / Y / Z / All, Home (`$H`), Unlock (`$X`)
- Feed-Hold / Resume / Soft-Reset (Stop) always visible
- Console of bytes to/from GRBL
- JOBS: list SD + flash jobs, Run with line/% progress, Pause / Resume / Stop
- WiFi: scan + on-screen keyboard, credentials in NVS; header shows mode + IP
- Web mirror at the pendant IP: same DRO/controls/jobs plus **G-code upload**, live WebSocket

### Three ways to run G-code

1. **Wired USB** — PC on the ESP32-S3 **native USB** CDC port. Char-counting streamer to GRBL. Real-time bytes (`?`, `!`, `~`, Ctrl-X, `0x85`) pass through; replies echo to the PC. Independent of WiFi.
2. **Stored job** — upload `.nc / .gcode / .ngc / .tap / .txt` from the web UI, Run from JOBS or the browser.
3. **Web stream** — `gcode` field in the browser console.

## Build and flash (pendant)

```bash
pio run
pio run -t upload --upload-port /dev/ttyACM0   # Linux
pio run -t upload --upload-port COM5           # Windows: Device Manager → USB serial
```

Use the ESP32-S3 **native USB** port for this firmware. That same port is the PC G-code feed at runtime. The CH343 “UART” USB-C on the Waveshare board is a different port.

`CORE_DEBUG_LEVEL=0` in `platformio.ini` keeps CDC quiet for a sender; bump it for bring-up logs. If upload will not sync: hold **BOOT**, tap **RESET**, release **BOOT**.

The web UI is compiled into `src/web_index.h` (PROGMEM). One `pio run -t upload` is the whole HMI — no extra filesystem image.

Display, GT911, backlight, and CH422G use Espressif `ESP32_Display_Panel` board `BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7`.

### Flash GRBL on the Uno first

Disconnect the pendant TX/RX (shared UART0). Flash GRBL 1.1 to the Uno over USB, then reconnect the shifter. Suggested settings:

- `$10=1` — status includes machine position + WCO
- `$13=0` — millimetres
- `$22=1` if you have homing switches; power-up **Alarm** → Unlock (`$X`) or Home (`$H`)

## WiFi / web

- No saved network: SoftAP `CNC-Pendant` / `cnc12345` → `http://192.168.4.1`, or join a STA from the WiFi screen. Change the AP password before a public demo.
- Join failure falls back to AP.
- HTTP :80, WebSocket :81.
- Compile out networking with `ENABLE_WIFI 0` in `src/config.h`.

## microSD

SPI `MOSI=11 / SCK=12 / MISO=13`, chip-select on **CH422G EXIO4** (not an ESP32 GPIO). Optional: if the card does not mount, jobs still run from LittleFS.

## Layout

| Path | Purpose |
|---|---|
| `platformio.ini` | build, board, libraries |
| `boards/BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7.json` | 16 MB flash / 8 MB OPI PSRAM |
| `partitions.csv` | partition table |
| `src/config.h` | pins, baud, jog, **feature toggles** |
| `src/grbl.*` | GRBL protocol, status, char-counting streamer |
| `src/machine.*` | state + command/log queues |
| `src/ui.*` | LVGL Control / Jobs / WiFi |
| `src/usbfeed.*` | native-USB CDC G-code bridge |
| `src/sdcard.*` | microSD, CS on CH422G EXIO4 |
| `src/jobstore.*` | SD + LittleFS jobs |
| `src/wifimgr.*` | STA + AP, NVS |
| `src/webserver.*` | HTTP + REST + WebSocket |
| `src/web_index.h` | embedded web UI |
| `src/main.cpp` | board/LVGL init, timers |

Toggles in `config.h`: `ENABLE_WIFI`, `ENABLE_WEB`, `ENABLE_STORAGE`, `ENABLE_SD`, `ENABLE_USB_FEED`.

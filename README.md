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
master to GRBL and renders the touch UI.

**Current scope — control pendant:**

- Color-coded status bar (Idle / Run / Jog / Hold / Alarm / Disconnected)
- Large X/Y/Z **DRO** with a work/machine (WPos/MPos) toggle
- **Jog** cross (X/Y/Z ±) with step-size (`0.01 / 0.1 / 1 / 10` mm) and feed-rate
  selectors, plus a Step/Continuous toggle and jog-cancel
- **Zero** X / Y / Z / All, **Home** (`$H`), **Unlock** (`$X`)
- Always-visible **Feed-Hold / Resume / Soft-Reset (Stop)** buttons
- Scrolling **console** of everything sent to and received from GRBL

**Roadmap:**

- Inline **USB-serial bridge** so a laptop sender (CNCjs) can run jobs *through*
  the pendant while the screen jogs and shows the DRO — no WiFi required
- Optional on-board job storage (flash / SD) and WiFi upload as later add-ons

### Build & flash

```bash
export PATH="$HOME/.local/bin:$PATH"
pio run                 # build
pio run -t upload       # build + flash
pio device monitor      # serial log (115200)
```

Flash over the board's **"UART" USB-C port** (with the UART switch on the USB
side). If upload can't sync, hold **BOOT**, tap **RESET**, release **BOOT**.

The display, GT911 touch, backlight, and CH422G IO-expander are handled by
Espressif's `ESP32_Display_Panel` supported-board profile
`BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7`.

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
| `src/config.h` | pins, baud, jog defaults, feature stubs — **edit me** |
| `src/grbl.*` | GRBL serial protocol + status parser |
| `src/ui.*` | LVGL touchscreen interface |
| `src/main.cpp` | board/LVGL init and service loop |
| `src/lvgl_v8_port.*`, `src/lv_conf.h`, `src/esp_panel_*` | display / LVGL porting (from Espressif's example) |

---

## Safety

This is machine-control software with **no e-stop guarantees**. Keep a physical
emergency stop that cuts motor power independently of the screen. The **Stop**
button issues a GRBL soft-reset (Ctrl-X) — it halts motion but is not a
substitute for cutting power.

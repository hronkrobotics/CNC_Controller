/*
 * sdcard.h - microSD access for the pendant (SPI, CS on CH422G EXIO4).
 *
 * VERIFIED AGAINST SCHEMATIC, NEEDS ON-HARDWARE VERIFICATION.
 *
 * On the Waveshare ESP32-S3-Touch-LCD-7 the SD card sits on an SPI bus
 * (MOSI=11, SCK=12, MISO=13) but its chip-select is NOT a GPIO - it is driven
 * by the CH422G IO expander on EXIO4. The card is the only device on that SPI
 * bus, so we assert EXIO4 low (select) via the expander and keep it asserted.
 *
 * The module is fully optional: if init fails (no card, wiring, etc.) the rest
 * of the firmware runs normally and the UI simply reports "SD unavailable".
 */
#pragma once

#include <Arduino.h>
#include <FS.h>

namespace esp_expander { class Base; }

// Initialise the SD card. `expander` is the CH422G obtained from the already
// initialised ESP32_Display_Panel Board (board->getIO_Expander()->getBase()).
// Returns true if a card mounted successfully.
bool      sd_init(esp_expander::Base *expander);
bool      sd_available();
fs::FS   &sd_fs();              // valid only when sd_available()
uint64_t  sd_total_bytes();
uint64_t  sd_used_bytes();

#include "sdcard.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>
#include <esp_io_expander.hpp>

static bool       s_ok = false;
static SPIClass   s_spi(FSPI);

bool sd_available()      { return s_ok; }
fs::FS &sd_fs()          { return SD; }
uint64_t sd_total_bytes(){ return s_ok ? SD.totalBytes() : 0; }
uint64_t sd_used_bytes() { return s_ok ? SD.usedBytes()  : 0; }

bool sd_init(esp_expander::Base *expander)
{
#if !ENABLE_SD
    return false;
#else
    s_ok = false;

    // 1) Assert the SD chip-select (active low) through the CH422G EXIO4. The
    //    card is the only device on this bus, so we hold CS asserted for the
    //    whole session. NEEDS ON-HARDWARE VERIFICATION.
    if (expander) {
        expander->pinMode(SD_CS_EXPANDER_PIN, OUTPUT);
        expander->digitalWrite(SD_CS_EXPANDER_PIN, LOW);
    }

    // 2) Bring up the SPI bus on the schematic pins.
    s_spi.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, -1);

    // 3) Mount. The real CS lives on the CH422G (asserted low above) and the
    //    SD card is the only device on this bus, so the Arduino SD driver is
    //    given a "no GPIO" chip-select (255 == none). On hardware the card
    //    stays selected via EXIO4, so the driver's inability to toggle a CS pin
    //    is harmless (it logs a benign "Invalid IO 255" at CORE_DEBUG_LEVEL>0).
    //    NEEDS ON-HARDWARE VERIFICATION - if a real card won't mount this way,
    //    switch to the ESP-IDF esp_vfs_fat_sdspi_mount() path with
    //    slot_config.gpio_cs = GPIO_NUM_NC (keeps EXIO4 as the physical CS).
    if (!SD.begin((uint8_t)-1, s_spi, SD_SPI_FREQ_HZ, "/sd")) {
        // Leave CS deasserted if we failed to mount.
        if (expander) expander->digitalWrite(SD_CS_EXPANDER_PIN, HIGH);
        return false;
    }

    if (SD.cardType() == CARD_NONE) return false;
    s_ok = true;
    return true;
#endif
}

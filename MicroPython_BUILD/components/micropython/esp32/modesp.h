
#include "esp_spi_flash.h"
#include "wear_levelling.h"
#include "driver/sdmmc_types.h"

void esp_neopixel_write(uint8_t pin, uint8_t *pixels, uint32_t numBytes, uint8_t timing);
const esp_partition_t fs_part;
void sdcard_print_info(const sdmmc_card_t* card, int mode);

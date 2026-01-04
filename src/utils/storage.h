#pragma once

#include <stdint.h>
#include <stdbool.h>

bool is_sdcard_inserted();
uint32_t get_nand_available_size();
uint32_t get_sd_available_size();

uint32_t get_nand_total_size();
uint32_t get_sd_total_size();

int format_sd_card();
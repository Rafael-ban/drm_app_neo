#pragma once

#include <stddef.h>
#include <stdint.h>

void get_meminfo_str(char *ret, size_t ret_sz);
void get_os_release_str(char *ret, size_t ret_sz);
uint32_t get_file_crc32(const char *path);
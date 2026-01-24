#pragma once

#include "utils/cJSON.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

uint64_t get_now_us(void);
void fill_nv12_buffer_with_color(uint8_t* buf, int width, int height, uint32_t rgb);
void safe_strcpy(char *dst, size_t dst_sz, const char *src) ;
int join_path(char *dst, size_t dst_sz, const char *base, const char *rel) ;
const char* path_basename(const char *path) ;
int file_exists_readable(const char *filepath) ;
int file_exists_executable(const char *filepath) ;
void set_lvgl_path(char *dst, size_t dst_sz, const char *abs_path) ;
char* read_file_all(const char *filepath, size_t *out_len) ;
const char* json_get_string(cJSON *obj, const char *key) ;
int json_get_int(cJSON *obj, const char *key, int def) ;
bool json_get_bool(cJSON *obj, const char *key, bool def) ;
uint32_t parse_rgbff(const char *hex) ;
int is_hex_color_6(const char *s) ;
bool is_sdcard_inserted();
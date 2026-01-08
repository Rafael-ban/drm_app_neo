#pragma once
#include "config.h"
#include "stdint.h"

typedef struct {
    char operator_name[40];
    char operator_name[40];
    uint128_t

} prts_operator_entry_t;

typedef struct {
    int exitcode;
} prts_t;

prts_t g_prts;

void prts_init();
void prts_destroy();

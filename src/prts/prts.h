#pragma once
#include "config.h"
#include "stdint.h"
#include "utils/uuid.h"
#include "overlay/opinfo.h"
#include "overlay/transitions.h"
#include <stdio.h>

typedef enum {
    PARSE_LOG_ERROR = 0,
    PARSE_LOG_WARN = 1,
} prts_parse_log_type_t;

typedef enum {
    DISPLAY_360_640 = 0,
    DISPLAY_480_854 = 1,
    DISPLAY_720_1280 = 2,
} display_type_t;

typedef struct {
    char path[128];

    // only valid in intro:
    bool enabled;
    int duration;
} prts_video_t;

typedef struct {
    char operator_name[40];
    uuid_t uuid;
    char description[256];
    char icon_path[128];
    display_type_t disp_type;

    prts_video_t intro_video;
    prts_video_t loop_video;

    olopinfo_params_t opinfo_params;
    oltr_params_t transition_in;
    oltr_params_t transition_loop;

} prts_operator_entry_t;

typedef struct {
    prts_operator_entry_t operators[PRTS_OPERATORS_MAX];
    int operator_count;

    FILE* parse_log_f;

    prts_timer_handle_t timer_handle;
} prts_t;

void prts_init(prts_t* prts);
void prts_destroy(prts_t* prts);

void prts_log_parse_log(prts_t* prts,char* path,char* message,prts_parse_log_type_t type);

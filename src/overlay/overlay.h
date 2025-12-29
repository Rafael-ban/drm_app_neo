#pragma once
#include "drm_warpper.h"


typedef struct {
    drm_warpper_t* drm_warpper;

    buffer_object_t overlay_buf_1;
    buffer_object_t overlay_buf_2;
    drm_warpper_queue_item_t overlay_buf_1_item;
    drm_warpper_queue_item_t overlay_buf_2_item;

} overlay_t;
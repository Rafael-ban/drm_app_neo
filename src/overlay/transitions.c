#include <stdint.h>

#include "utils/log.h"
#include "config.h"
#include "overlay/overlay.h"
#include "overlay/transitions.h"
#include "driver/drm_warpper.h"
#include "utils/timer.h"
#include "render/layer_animation.h"

#define STB_IMAGE_IMPLEMENTATION
#include "utils/stb_image.h"

// 渐变过渡，准备完成后无耗时操作，不需要使用worker。
void overlay_transition_fade(overlay_t* overlay,void (*middle_cb)(void *userdata,bool is_last),void* userdata,oltr_params_t* params){
    drm_warpper_queue_item_t* item;

    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0);

    // 此处亦有等待vsync的功能。
    // get a free buffer to draw on

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;

    int w,h,c;
    uint8_t* pixdata = stbi_load(params->image_path, &w, &h, &c, 4);
    if(!pixdata){
        log_error("failed to load image: %s", params->image_path);
        w = 0;
        h = 0;
    }
    else if(w > OVERLAY_WIDTH || h > OVERLAY_HEIGHT){
        log_error("image size is too large: %s", params->image_path);
        w = 0;
        h = 0;
    }

    int image_start_x = UI_WIDTH / 2 - w / 2;
    int image_start_y = UI_HEIGHT / 2 - h / 2;

    for(int y=0; y<OVERLAY_HEIGHT; y++){
        for(int x=0; x<OVERLAY_WIDTH; x++){
            if(x >= image_start_x && x < image_start_x + w && y >= image_start_y && y < image_start_y + h){
                *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = *((uint32_t *)(pixdata) + (x - image_start_x) + (y - image_start_y) * w);
            }
            else{
                *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = params->background_color;
            }
        }
    }

    stbi_image_free(pixdata);

    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);


    // 渐变到图层
    layer_animation_fade_in(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        params->duration, 
        0
    );

    // 全部遮住以后挂载video层
    prts_timer_handle_t init_handler;
    prts_timer_create(&init_handler,params->duration,0,1,middle_cb,userdata);
    
    // 渐变到透明
    layer_animation_fade_out(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        params->duration, 
        2 * params->duration
    );

}

// 贝塞尔函数移动过渡。 不需要使用worker
void overlay_transition_move(overlay_t* overlay,void (*middle_cb)(void *userdata,bool is_last),void* userdata,oltr_params_t* params){

    drm_warpper_queue_item_t* item;

    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, SCREEN_WIDTH, 0);
    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);

    // 此处亦有等待vsync的功能。
    // get a free buffer to draw on

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;

    int w,h,c;
    uint8_t* pixdata = stbi_load(params->image_path, &w, &h, &c, 4);
    if(!pixdata){
        log_error("failed to load image: %s", params->image_path);
        w = 0;
        h = 0;
    }
    else if(w > OVERLAY_WIDTH || h > OVERLAY_HEIGHT){
        log_error("image size is too large: %s", params->image_path);
        w = 0;
        h = 0;
    }

    int image_start_x = UI_WIDTH / 2 - w / 2;
    int image_start_y = UI_HEIGHT / 2 - h / 2;

    for(int y=0; y<OVERLAY_HEIGHT; y++){
        for(int x=0; x<OVERLAY_WIDTH; x++){
            if(x >= image_start_x && x < image_start_x + w && y >= image_start_y && y < image_start_y + h){
                *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = *((uint32_t *)(pixdata) + (x - image_start_x) + (y - image_start_y) * w);
            }
            else{
                *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = params->background_color;
            }
        }
    }

    stbi_image_free(pixdata);

    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);


    layer_animation_ease_out_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        SCREEN_WIDTH, 0,
        0, 0,
        params->duration, 
        0
    );

    // 全部遮住以后挂载video层
    prts_timer_handle_t init_handler;
    prts_timer_create(&init_handler,params->duration,0,1,middle_cb,userdata);
    
    layer_animation_ease_in_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        0, 0,
        -SCREEN_WIDTH, 0,
        params->duration, 
        2 * params->duration
    );

}

typedef struct {
    overlay_t* overlay;
    prts_timer_handle_t timer_handle;
    int w;
    int h;
    int c;
    uint8_t* pixdata;
    int image_start_x;
    int image_start_y;
    uint32_t background_color;

    int curr_frame;
    int total_frames;
    int frame_step_y;

    // 在双缓冲环境下，需要记录上一次跳帧的数量，
    // 因为buffer是轮转的。
    int last_skipped_frames;

    bool middle_cb_called;
    void (*middle_cb)(void *userdata,bool is_last);
    void* userdata;
} swipe_worker_data_t;


// 每一帧绘制的时候来调用。 来自 overlay_worker 线程
// 我草，双缓冲+dirty rect是人写的吗 我就写这一次把 要死了
// 为了你的理智着想，我也不建议你来这么写
static void swipe_worker(void *userdata,int skipped_frames){
    swipe_worker_data_t* data = (swipe_worker_data_t*)userdata;
    log_trace("swipe_worker: skipped_frames=%d,curr_frame=%d,total_frames=%d", skipped_frames, data->curr_frame, data->total_frames);
    drm_warpper_set_layer_coord(data->overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0);
    drm_warpper_queue_item_t* item;
    drm_warpper_dequeue_free_item(data->overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;


    // 第一部分：先从空屏填充到完整内容
    if(data->curr_frame < data->total_frames / 3){
        // 双缓冲 要重画画在另一个buffer上的内容
        int draw_start_y = (data->curr_frame - data->last_skipped_frames - 1) * data->frame_step_y;
        int draw_end_y = draw_start_y + (2 + data->last_skipped_frames) * data->frame_step_y;
        if(draw_end_y > OVERLAY_HEIGHT){
            draw_end_y = OVERLAY_HEIGHT;
        }
        if(draw_start_y < 0){
            draw_start_y = 0;
        }

        // log_trace("first part: curr_frame=%d,draw_start_y=%d,draw_end_y=%d", data->curr_frame, draw_start_y, draw_end_y);

        for(int y=draw_start_y; y<draw_end_y; y++){
            for(int x=0; x<OVERLAY_WIDTH; x++){
                if(x >= data->image_start_x && x < data->image_start_x + data->w && y >= data->image_start_y && y < data->image_start_y + data->h){
                    *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = *((uint32_t *)(data->pixdata) + (x - data->image_start_x) + (y - data->image_start_y) * data->w);
                }
                else{
                    *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = data->background_color;
                }
            }
        }
    }
    // 第二部分：保持
    else if(data->curr_frame < 2 * data->total_frames / 3){

        // 强制补画三帧，因为最后一帧是空的，需要补上。
        int draw_start_y = OVERLAY_HEIGHT - data->frame_step_y * 3;
        int draw_end_y = OVERLAY_HEIGHT;

        for(int y=draw_start_y; y<draw_end_y; y++){
            for(int x=0; x<OVERLAY_WIDTH; x++){
                if(x >= data->image_start_x && x < data->image_start_x + data->w && y >= data->image_start_y && y < data->image_start_y + data->h){
                    *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = *((uint32_t *)(data->pixdata) + (x - data->image_start_x) + (y - data->image_start_y) * data->w);
                }
                else{
                    *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = data->background_color;
                }
            }
        }

        if(!data->middle_cb_called){
            data->middle_cb_called = true;
            data->middle_cb(data->userdata,false);
        }
    }
    // 第三部分，从完整内容填充到空屏
    else if(data->curr_frame < data->total_frames){
        int draw_start_y = (data->curr_frame - data->last_skipped_frames - 2 * data->total_frames / 3 - 1) * data->frame_step_y;
        int draw_end_y = draw_start_y + (2 + data->last_skipped_frames) * data->frame_step_y;
        if(draw_end_y > OVERLAY_HEIGHT){
            draw_end_y = OVERLAY_HEIGHT;
        }
        if(draw_start_y < 0){
            draw_start_y = 0;
        }

        // log_trace("third part: draw_start_y=%d,draw_end_y=%d", draw_start_y, draw_end_y);

        for(int y=draw_start_y; y<draw_end_y; y++){
            for(int x=0; x<OVERLAY_WIDTH; x++){
                *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = 0;
            }
        }
    }

    if(data->curr_frame >= data->total_frames - skipped_frames - 1){
        // 强制补画三帧，因为最后一帧是空的，需要补上。
        log_info("force draw last 3 frames");
        int draw_start_y = OVERLAY_HEIGHT - data->frame_step_y * 3;
        int draw_end_y = OVERLAY_HEIGHT;

        for(int y=draw_start_y; y<draw_end_y; y++){
            for(int x=0; x<OVERLAY_WIDTH; x++){
                *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = 0;
            }
        }

    }
    drm_warpper_enqueue_display_item(data->overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);

    data->last_skipped_frames = skipped_frames;
    data->curr_frame += skipped_frames + 1;

    if(data->curr_frame >= data->total_frames){

        log_info("swipe worker finished");
        prts_timer_cancel(data->timer_handle);
        stbi_image_free(data->pixdata);
    }
}

// 定时器回调。来自普瑞塞斯 的 rt 启动的 sigev_thread 线程。
static void swipe_timer_cb(void *userdata,bool is_last){
    swipe_worker_data_t* data = (swipe_worker_data_t*)userdata;
    overlay_worker_schedule(data->overlay,swipe_worker,data);
}

// 类似drm_app的过渡效果，需要使用worker。
void overlay_transition_swipe(overlay_t* overlay,void (*middle_cb)(void *userdata,bool is_last),void* userdata,oltr_params_t* params){
    drm_warpper_queue_item_t* item;
    uint32_t* vaddr;

    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0);

    // 清空双缓冲buffer
    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    vaddr = (uint32_t*)item->mount.arg0;
    memset(vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);
    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    vaddr = (uint32_t*)item->mount.arg0;
    memset(vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);
    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);
    

    drm_warpper_mount_layer(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0, &overlay->overlay_buf_1);

    int w,h,c;
    uint8_t* pixdata = stbi_load(params->image_path, &w, &h, &c, 4);
    if(!pixdata){
        log_error("failed to load image: %s", params->image_path);
        w = 0;
        h = 0;
    }
    else if(w > OVERLAY_WIDTH || h > OVERLAY_HEIGHT){
        log_error("image size is too large: %s", params->image_path);
        w = 0;
        h = 0;
    }

    static swipe_worker_data_t swipe_worker_data;
    swipe_worker_data.overlay = overlay;
    swipe_worker_data.w = w;
    swipe_worker_data.h = h;
    swipe_worker_data.c = c;
    swipe_worker_data.pixdata = pixdata;
    swipe_worker_data.image_start_x = UI_WIDTH / 2 - w / 2;
    swipe_worker_data.image_start_y = UI_HEIGHT / 2 - h / 2;
    swipe_worker_data.curr_frame = 0;
    swipe_worker_data.total_frames = 3 * params->duration / LAYER_ANIMATION_STEP_TIME;
    swipe_worker_data.frame_step_y = OVERLAY_HEIGHT / (params->duration / LAYER_ANIMATION_STEP_TIME);
    swipe_worker_data.background_color = params->background_color;
    swipe_worker_data.middle_cb = middle_cb;
    swipe_worker_data.userdata = userdata;


    // 我们在这里设置永远触发，其实是在worker里面注销定时器。
    // 我们要保证，就算有跳帧发生，最后一次触发的事件也能传到我们的回调里面
    // 在那里处理资源回收的问题。
    prts_timer_create(
        &swipe_worker_data.timer_handle,
        params->duration,
        LAYER_ANIMATION_STEP_TIME,
        -1,
        swipe_timer_cb,
        &swipe_worker_data
    );

}
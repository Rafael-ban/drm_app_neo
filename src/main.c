#include "drm_warpper.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "log.h"
#include <signal.h>
#include "mediaplayer.h"


drm_warpper_t g_drm_warpper;
mediaplayer_t g_mediaplayer;

static int g_running = 1;
void signal_handler(int sig)
{
    log_info("received signal %d, shutting down", sig);
    g_running = 0;
}

int main(int argc, char *argv[]){
    if(argc == 2){
        if(strcmp(argv[1], "version") == 0){
            log_info("EPASS_GIT_VERSION: %s", EPASS_GIT_VERSION);
            return 0;
        }
        else if(strcmp(argv[1], "aux") == 0){
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if(drm_warpper_init(&g_drm_warpper) != 0){
        log_error("failed to initialize DRM warpper");
        return -1;
    }

    // drm_warpper_init_layer(
    //     &g_drm_warpper, 
    //     DRM_WARPPER_LAYER_VIDEO, 
    //     VIDEO_WIDTH, 
    //     VIDEO_HEIGHT, 
    // DRM_WARPPER_LAYER_MODE_MB32_NV12);
    
    // drm_warpper_mount_layer(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, 0, 0);

    /* initialize mediaplayer */
    // if (mediaplayer_init(&g_mediaplayer) != 0) {
    //     log_error("failed to initialize mediaplayer");
    //     drm_warpper_destroy(&g_drm_warpper);
    //     return -1;
    // }

    // mediaplayer_set_output_buffer(&g_mediaplayer, g_drm_warpper.plane[DRM_WARPPER_LAYER_VIDEO].buf[0].vaddr);
    // mediaplayer_set_video(&g_mediaplayer, "/assets/MS/loop.mp4");
    // mediaplayer_start(&g_mediaplayer);

    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_VIDEO, 
        VIDEO_WIDTH, 
        VIDEO_HEIGHT, 
    DRM_WARPPER_LAYER_MODE_MB32_NV12);

    // FIXME：
    // 用来跑modeset的buffer，实际上是不用的，这一片内存你也可以拿去干别的
    // 期待有能人帮优化掉这个allocate。
    buffer_object_t video_buf;
    drm_warpper_allocate_buffer(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, &video_buf);
    drm_warpper_mount_layer(&g_drm_warpper, DRM_WARPPER_LAYER_VIDEO, 0, 0, &video_buf);

    /* initialize mediaplayer */
    if (mediaplayer_init(&g_mediaplayer, &g_drm_warpper) != 0) {
        log_error("failed to initialize mediaplayer");
        drm_warpper_destroy(&g_drm_warpper);
        return -1;
    }

    mediaplayer_set_video(&g_mediaplayer, "/root/loop.mp4");
    mediaplayer_start(&g_mediaplayer);



    drm_warpper_init_layer(
        &g_drm_warpper, 
        DRM_WARPPER_LAYER_UI, 
        UI_WIDTH, UI_HEIGHT, 
        DRM_WARPPER_LAYER_MODE_ARGB8888
    );
    // drm_warpper_mount_layer(&g_drm_warpper, DRM_WARPPER_LAYER_UI, 0, 0);
    buffer_object_t ui_buf_1;
    buffer_object_t ui_buf_2;
    drm_warpper_allocate_buffer(&g_drm_warpper, DRM_WARPPER_LAYER_UI, &ui_buf_1);
    drm_warpper_allocate_buffer(&g_drm_warpper, DRM_WARPPER_LAYER_UI, &ui_buf_2);

    drm_warpper_mount_layer(&g_drm_warpper, DRM_WARPPER_LAYER_UI, 0, 0, &ui_buf_1);
    
    drm_warpper_queue_item_t ui_buf_1_item;
    drm_warpper_queue_item_t ui_buf_2_item;
    ui_buf_1_item.mount.type = DRM_SRGN_MOUNT_FB_TYPE_NORMAL;
    ui_buf_1_item.mount.ch0_addr = (uint32_t)ui_buf_1.vaddr;
    ui_buf_1_item.mount.ch1_addr = 0;
    ui_buf_1_item.mount.ch2_addr = 0;
    ui_buf_1_item.userdata = (void*)&ui_buf_1;

    ui_buf_2_item.mount.type = DRM_SRGN_MOUNT_FB_TYPE_NORMAL;
    ui_buf_2_item.mount.ch0_addr = (uint32_t)ui_buf_2.vaddr;
    ui_buf_2_item.mount.ch1_addr = 0;
    ui_buf_2_item.mount.ch2_addr = 0;
    ui_buf_2_item.userdata = (void*)&ui_buf_2;

    // 先把buffer提交进去，形成队列的初始状态（有一个buffer等待被free回来）
    drm_warpper_enqueue_display_item(&g_drm_warpper, DRM_WARPPER_LAYER_UI, &ui_buf_1_item);
    drm_warpper_enqueue_display_item(&g_drm_warpper, DRM_WARPPER_LAYER_UI, &ui_buf_2_item);

    while(g_running){
        drm_warpper_queue_item_t* item;
        buffer_object_t* buf;
        drm_warpper_dequeue_free_item(&g_drm_warpper, DRM_WARPPER_LAYER_UI, &item);
        buf = (buffer_object_t*)item->userdata;
        // log_info("get free buffer to draw: %p", buf->vaddr);

        for(int i = 0; i < 100; i++){
            for(int j = 0; j < 100; j++){
                *((uint32_t*)buf->vaddr + i * buf->width + j) = 0xff00ffcc;
            }
        }

        drm_warpper_enqueue_display_item(&g_drm_warpper, DRM_WARPPER_LAYER_UI, item);

        // getchar();

        drm_warpper_dequeue_free_item(&g_drm_warpper, DRM_WARPPER_LAYER_UI, &item);
        buf = (buffer_object_t*)item->userdata;
        // log_info("get free buffer to draw: %p", buf->vaddr);

        for(int i = 0; i < 100; i++){
            for(int j = 0; j < 100; j++){
                *((uint32_t*)buf->vaddr + i * buf->width + j) = 0xffff0000;
            }
        }

        drm_warpper_enqueue_display_item(&g_drm_warpper, DRM_WARPPER_LAYER_UI, item);

        // getchar();

    }

    /* cleanup */
    log_info("shutting down");

    mediaplayer_stop(&g_mediaplayer);
    mediaplayer_destroy(&g_mediaplayer);
    drm_warpper_destroy(&g_drm_warpper);

    return 0;
}
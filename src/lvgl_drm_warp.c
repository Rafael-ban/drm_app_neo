#include "lvgl_drm_warp.h"
#include "config.h"
#include "log.h"
#include <time.h>
#include <unistd.h>
#include "gui_app.h"

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}

static void lvgl_drm_warp_flush_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{

    if(!lv_disp_flush_is_last(disp_drv)){
        lv_disp_flush_ready(disp_drv);
        return;
    }
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one
     *`put_px` is just an example, it needs to implemented by you.*/
    lvgl_drm_warp_t *lvgl_drm_warp = (lvgl_drm_warp_t *)disp_drv->user_data;

    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing*/
    if(lvgl_drm_warp->curr_draw_buf_idx == 0){
        drm_warpper_enqueue_display_item(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_1_item);
    }else{
        drm_warpper_enqueue_display_item(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_2_item);
    }
    lvgl_drm_warp->curr_draw_buf_idx = !lvgl_drm_warp->curr_draw_buf_idx;

    // save invalid areas
    lvgl_drm_warp->inv_cnt = 0;
    lv_disp_t * disp = _lv_refr_get_disp_refreshing();
    for(int i = 0; i < disp->inv_p; i++){
        if(disp->inv_area_joined[i]){
            continue;
        }
        lvgl_drm_warp->inv_areas[lvgl_drm_warp->inv_cnt++] = disp->inv_areas[i];
    }
    lv_disp_flush_ready(disp_drv);
}

static void lvgl_drm_warp_render_start_cb(lv_disp_drv_t * disp_drv){
    drm_warpper_queue_item_t* item;
    lvgl_drm_warp_t *lvgl_drm_warp = (lvgl_drm_warp_t *)disp_drv->user_data;
    drm_warpper_dequeue_free_item(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &item);
    uint32_t* target_vaddr = (uint32_t*)(lvgl_drm_warp->curr_draw_buf_idx == 0 ? lvgl_drm_warp->ui_buf_1.vaddr : lvgl_drm_warp->ui_buf_2.vaddr);
    uint32_t* src_vaddr = (uint32_t*)(lvgl_drm_warp->curr_draw_buf_idx == 0 ? lvgl_drm_warp->ui_buf_2.vaddr : lvgl_drm_warp->ui_buf_1.vaddr);
    for(int i = 0; i < lvgl_drm_warp->inv_cnt; i++){
        lv_area_t* area = &lvgl_drm_warp->inv_areas[i];
        for(int y = area->y1; y < area->y2; y++){
            for(int x = area->x1; x < area->x2; x++){
                target_vaddr[y * UI_WIDTH + x] = src_vaddr[y * UI_WIDTH + x];
            }
        }
    }
}

void lvgl_drm_warp_init(lvgl_drm_warp_t *lvgl_drm_warp,drm_warpper_t *drm_warpper){

    lvgl_drm_warp->drm_warpper = drm_warpper;

    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_1);
    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_2);

    // modeset
    drm_warpper_mount_layer(drm_warpper, DRM_WARPPER_LAYER_UI, 0, 0, &lvgl_drm_warp->ui_buf_1);

    lvgl_drm_warp->ui_buf_1_item.mount.type = DRM_SRGN_MOUNT_FB_TYPE_NORMAL;
    lvgl_drm_warp->ui_buf_1_item.mount.ch0_addr = (uint32_t)lvgl_drm_warp->ui_buf_1.vaddr;
    lvgl_drm_warp->ui_buf_1_item.mount.ch1_addr = 0;
    lvgl_drm_warp->ui_buf_1_item.mount.ch2_addr = 0;
    lvgl_drm_warp->ui_buf_1_item.userdata = (void*)&lvgl_drm_warp->ui_buf_1;

    lvgl_drm_warp->ui_buf_2_item.mount.type = DRM_SRGN_MOUNT_FB_TYPE_NORMAL;
    lvgl_drm_warp->ui_buf_2_item.mount.ch0_addr = (uint32_t)lvgl_drm_warp->ui_buf_2.vaddr;
    lvgl_drm_warp->ui_buf_2_item.mount.ch1_addr = 0;
    lvgl_drm_warp->ui_buf_2_item.mount.ch2_addr = 0;
    lvgl_drm_warp->ui_buf_2_item.userdata = (void*)&lvgl_drm_warp->ui_buf_2;

    // 先把buffer提交进去，形成队列的初始状态（有一个buffer等待被free回来）
    drm_warpper_enqueue_display_item(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_1_item);
    drm_warpper_enqueue_display_item(drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_2_item);

    lv_init();
    lvgl_drm_warp->curr_draw_buf_idx = 0;

    lv_disp_draw_buf_init(&lvgl_drm_warp->draw_buf, 
        lvgl_drm_warp->ui_buf_1.vaddr, 
        lvgl_drm_warp->ui_buf_2.vaddr, 
    UI_WIDTH * UI_HEIGHT*4);

    lv_disp_drv_init(&lvgl_drm_warp->disp_drv);            /*Basic initialization*/
    lvgl_drm_warp->disp_drv.draw_buf = &lvgl_drm_warp->draw_buf;          /*Set an initialized buffer*/
    lvgl_drm_warp->disp_drv.flush_cb = lvgl_drm_warp_flush_cb;        /*Set a flush callback to draw to the display*/
    lvgl_drm_warp->disp_drv.hor_res = UI_WIDTH;                 /*Set the horizontal resolution in pixels*/
    lvgl_drm_warp->disp_drv.ver_res = UI_HEIGHT;                 /*Set the vertical resolution in pixels*/
    lvgl_drm_warp->disp_drv.user_data = (void*)lvgl_drm_warp;
    lvgl_drm_warp->disp_drv.render_start_cb = lvgl_drm_warp_render_start_cb;
    lvgl_drm_warp->disp_drv.direct_mode = true;
    lvgl_drm_warp->disp_drv.screen_transp = 0;

    lvgl_drm_warp->disp = lv_disp_drv_register(&lvgl_drm_warp->disp_drv); /*Register the driver and save the created display objects*/



    gui_app_create_ui();

}

void lvgl_drm_warp_destroy(lvgl_drm_warp_t *lvgl_drm_warp){
    drm_warpper_free_buffer(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_1);
    drm_warpper_free_buffer(lvgl_drm_warp->drm_warpper, DRM_WARPPER_LAYER_UI, &lvgl_drm_warp->ui_buf_2);
}

void lvgl_drm_warp_tick(lvgl_drm_warp_t *lvgl_drm_warp){
    lv_timer_handler();
}
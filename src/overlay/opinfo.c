
#include "overlay/opinfo.h"
#include "driver/drm_warpper.h"
#include "utils/log.h"
#include "utils/timer.h"
#include "utils/stb_image.h"
#include "config.h"
#include "render/fbdraw.h"
#include "utils/cacheassets.h"

LV_FONT_DECLARE(ui_font_bebas_40);
LV_FONT_DECLARE(ui_font_sourcesans_reg_14);


#include <string.h>

void overlay_opinfo_show_image(overlay_t* overlay,olopinfo_params_t* params){
    drm_warpper_queue_item_t* item;

    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, OVERLAY_HEIGHT);

    // 此处亦有等待vsync的功能。
    // get a free buffer to draw on

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;


    int w,h,c;
    uint8_t* pixdata = stbi_load(params->image_path, &w, &h, &c, 4);
    if(!pixdata){
        log_error("failed to load image: %s", params->image_path);
    }
    else if(w != OVERLAY_WIDTH || h != OVERLAY_HEIGHT){
        log_error("image size is not correct: %s", params->image_path);
        stbi_image_free(pixdata);
        pixdata = NULL;
    }

    if(pixdata){
        for (int y = 0; y < OVERLAY_HEIGHT; y++) {
            for (int x = 0; x < OVERLAY_WIDTH; x++) {
                uint32_t bgra_pixel = *((uint32_t *)(pixdata) + x + y * w);
                uint32_t rgb_pixel = (bgra_pixel & 0x000000FF) << 16 | (bgra_pixel & 0x0000FF00) | (bgra_pixel & 0x00FF0000) >> 16 | (bgra_pixel & 0xFF000000);
                *((uint32_t *)(vaddr) + x + y * OVERLAY_WIDTH) = rgb_pixel;
            }
        }
        stbi_image_free(pixdata);
        pixdata = NULL;
    }
    else{   
        memset(vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);
    }

    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);

    layer_animation_ease_in_out_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        0, OVERLAY_HEIGHT,
        0, 0,
        params->fade_duration, 0
    );
}


typedef struct {
    overlay_t* overlay;

} arknights_overlay_worker_data_t;
// 绘制arknights overlay的worker.
// 不处理跳帧了。
static void arknights_overlay_worker(void *userdata,int skipped_frames){


}

static void init_template_arknights_overlay(uint32_t* vaddr){
    fbdraw_fb_t fbsrc,fbdst;
    fbdraw_rect_t src_rect,dst_rect;

    memset(vaddr, 0, OVERLAY_WIDTH * OVERLAY_HEIGHT * 4);


    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;


    int asset_w,asset_h;
    uint8_t* asset_addr;

    // TOP_LEFT_RECT
    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_LEFT_RECT, &asset_w, &asset_h, &asset_addr);

    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = OVERLAY_ARKNIGHTS_RECT_OFFSET_X;
    dst_rect.y = 0;
    dst_rect.w = OVERLAY_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // BTM_LEFT_BAR
    cacheassets_get_asset_from_global(CACHE_ASSETS_BTM_LEFT_BAR, &asset_w, &asset_h, &asset_addr);
    
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = 0;
    dst_rect.y = OVERLAY_HEIGHT - asset_h;
    dst_rect.w = OVERLAY_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT;
    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // TOP_RIGHT_BAR
    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_RIGHT_BAR, &asset_w, &asset_h, &asset_addr);
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = OVERLAY_WIDTH - asset_w;
    dst_rect.y = 0;
    dst_rect.w = OVERLAY_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT;
    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // AK_BAR
    cacheassets_get_asset_from_global(CACHE_ASSETS_AK_BAR, &asset_w, &asset_h, &asset_addr);
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
    dst_rect.y = OVERLAY_ARKNIGHTS_AK_BAR_OFFSET_Y;
    dst_rect.w = OVERLAY_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // TOP_LEFT_RHODES
    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_LEFT_RHODES, &asset_w, &asset_h, &asset_addr);
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.w = OVERLAY_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    // TOP_RIGHT_ARROW

    cacheassets_get_asset_from_global(CACHE_ASSETS_TOP_RIGHT_ARROW, &asset_w, &asset_h, &asset_addr);
    fbsrc.vaddr = (uint32_t*)asset_addr;
    fbsrc.width = asset_w;
    fbsrc.height = asset_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = asset_w;
    src_rect.h = asset_h;

    dst_rect.x = OVERLAY_WIDTH - asset_w;
    dst_rect.y = OVERLAY_ARKNIGHTS_TOP_RIGHT_ARROW_OFFSET_Y;
    dst_rect.w = OVERLAY_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);
}


void overlay_opinfo_show_arknights(overlay_t* overlay,olopinfo_params_t* params){
    drm_warpper_queue_item_t* item;
    fbdraw_fb_t fbdst,fbsrc;
    fbdraw_rect_t dst_rect,src_rect;

    drm_warpper_set_layer_alpha(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 255);
    drm_warpper_set_layer_coord(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, OVERLAY_HEIGHT);

    // 此处亦有等待vsync的功能。
    // get a free buffer to draw on

    drm_warpper_dequeue_free_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &item);
    uint32_t* vaddr = (uint32_t*)item->mount.arg0;


    init_template_arknights_overlay(vaddr);

    int radius = 192;
    uint32_t color = params->color;

    for(int x=0; x < radius; x++){
        for(int y=0; y < radius; y++){
            if(x+y > radius - 2){
                break;
            }
            uint8_t alpha = 255 - ((x+y)*255 / radius);
            int real_x = OVERLAY_WIDTH - x;
            int real_y = OVERLAY_HEIGHT - y;
            *((uint32_t *)(vaddr) + real_x + real_y * OVERLAY_WIDTH) = color | (alpha << 24);

        }
    }

    fbdst.vaddr = vaddr;
    fbdst.width = OVERLAY_WIDTH;
    fbdst.height = OVERLAY_HEIGHT;

    dst_rect.x = 1;
    dst_rect.y = OVERLAY_ARKNIGHTS_BARCODE_OFFSET_Y;
    dst_rect.w = OVERLAY_ARKNIGHTS_BARCODE_WIDTH;
    dst_rect.h = OVERLAY_ARKNIGHTS_BARCODE_HEIGHT;

    fbdraw_barcode_rot90(&fbdst, &dst_rect, params->barcode_text, &ui_font_sourcesans_reg_14);

    dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
    dst_rect.y = OVERLAY_ARKNIGHTS_OPNAME_OFFSET_Y;
    dst_rect.w = OVERLAY_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT;

    fbdraw_text(&fbdst, &dst_rect, params->operator_name, &ui_font_bebas_40, 0xFFFFFFFF,0);

    dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
    dst_rect.y = OVERLAY_ARKNIGHTS_UPPERLINE_OFFSET_Y;
    dst_rect.w = OVERLAY_ARKNIGHTS_LINE_WIDTH;
    dst_rect.h = 1;

    fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);

    dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
    dst_rect.y = OVERLAY_ARKNIGHTS_LOWERLINE_OFFSET_Y;
    dst_rect.w = OVERLAY_ARKNIGHTS_LINE_WIDTH;
    dst_rect.h = 1;

    fbdraw_fill_rect(&fbdst, &dst_rect, 0xFFFFFFFF);

    dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
    dst_rect.y = OVERLAY_ARKNIGHTS_OPCODE_OFFSET_Y;
    dst_rect.w = OVERLAY_ARKNIGHTS_LINE_WIDTH;
    dst_rect.h = 20;

    fbdraw_text(&fbdst, &dst_rect, params->operator_code, &ui_font_sourcesans_reg_14, 0xFFFFFFFF,0);

    dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
    dst_rect.y = OVERLAY_ARKNIGHTS_STAFF_TEXT_OFFSET_Y;
    dst_rect.w = OVERLAY_ARKNIGHTS_LINE_WIDTH;
    dst_rect.h = 20;

    fbdraw_text(&fbdst, &dst_rect, params->staff_text, &ui_font_sourcesans_reg_14, 0xFFFFFFFF,0);

    dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
    dst_rect.y = OVERLAY_ARKNIGHTS_CLASS_ICON_OFFSET_Y;
    dst_rect.w = OVERLAY_ARKNIGHTS_CLASS_ICON_WIDTH;
    dst_rect.h = OVERLAY_ARKNIGHTS_CLASS_ICON_HEIGHT;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = params->class_w;
    src_rect.h = params->class_h;
    
    fbsrc.vaddr = (uint32_t*)params->class_addr;
    fbsrc.width = params->class_w;
    fbsrc.height = params->class_h;

    fbdraw_copy_rect(&fbsrc, &fbdst, &src_rect, &dst_rect);

    

    dst_rect.x = OVERLAY_ARKNIGHTS_BTM_INFO_OFFSET_X;
    dst_rect.y = OVERLAY_ARKNIGHTS_AUX_TEXT_OFFSET_Y;
    dst_rect.w = OVERLAY_ARKNIGHTS_LINE_WIDTH;
    dst_rect.h = OVERLAY_HEIGHT - OVERLAY_ARKNIGHTS_AUX_TEXT_OFFSET_Y;

    fbdraw_text(&fbdst, &dst_rect, params->aux_text, &ui_font_sourcesans_reg_14, 0xFFFFFFFF,OVERLAY_ARKNIGHTS_AUX_TEXT_LINE_HEIGHT);


    fbsrc.vaddr = (uint32_t*)params->logo_addr;
    fbsrc.width = params->logo_w;
    fbsrc.height = params->logo_h;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = params->logo_w;
    src_rect.h = params->logo_h;
    
    dst_rect.x = OVERLAY_WIDTH - params->logo_w - 10;
    dst_rect.y = OVERLAY_HEIGHT - params->logo_h - 10;
    dst_rect.w = params->logo_w;
    dst_rect.h = params->logo_h;

    fbdraw_alpha_opacity_rect(&fbsrc, &fbdst, &src_rect, &dst_rect, 255);

    drm_warpper_enqueue_display_item(overlay->drm_warpper, DRM_WARPPER_LAYER_OVERLAY, item);

    layer_animation_ease_in_out_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        0, OVERLAY_HEIGHT,
        0, 0,
        params->fade_duration, 0
    );
}

void overlay_opinfo_stop(overlay_t* overlay){
    if(overlay->overlay_timer_handle){
        prts_timer_cancel(overlay->overlay_timer_handle);
        overlay->overlay_timer_handle = 0;
    }

    layer_animation_ease_in_out_move(
        overlay->layer_animation, 
        DRM_WARPPER_LAYER_OVERLAY, 
        0, 0,
        0, -OVERLAY_HEIGHT,
        UI_LAYER_ANIMATION_DURATION, 0
    );
}
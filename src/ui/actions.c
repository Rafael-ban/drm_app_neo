#include "actions.h"
#include "vars.h"
#include <stdint.h>
#include "log.h"
#include "layer_animation.h"
#include "config.h"
#include "stdlib.h"
#include "lvgl_drm_warp.h"
#include "ui.h"
#include "settings.h"
#include "storage.h"
#include "sysinfo.h"

extern settings_t g_settings;

const char *get_var_epass_version(){
    return EPASS_GIT_VERSION;
}
void set_var_epass_version(const char *value){
    return;
}


const char *get_var_sysinfo(){
    static char buf[2048];
    char meminfo[512] = {0};
    char osrelease[512] = {0};
    static int called_cnt = 0;
    if(called_cnt != 0){
        return buf;
    }
    called_cnt++;

    get_meminfo_str(meminfo, sizeof(meminfo));
    get_os_release_str(osrelease, sizeof(osrelease));

    uint32_t app_crc32 = get_file_crc32("/root/epass_drm_app");

    snprintf(
        buf, sizeof(buf), 
        "罗德岛电子通行认证程序-代号:%s\n"
        "版本号: %s\n"
        "校验码: %08X\n"
        "程序生成时间: %s\n"
        "%s"
        "%s\n",
        APP_SUBCODENAME,
        EPASS_GIT_VERSION,
        app_crc32,
        COMPILE_TIME,
        meminfo,
        osrelease
    );

    // refresh every 300 calls(10 secs)
    if(called_cnt == 300){
        called_cnt = 0;
    }
    return buf;
}
void set_var_sysinfo(const char *value){
    return;
}

sw_mode_t get_var_sw_mode(){
    return g_settings.switch_mode;
}
void set_var_sw_mode(sw_mode_t value){
    g_settings.switch_mode = value;
    settings_update(&g_settings);
    return;
}

sw_interval_t get_var_sw_interval(){
    return g_settings.switch_interval;
}
void set_var_sw_interval(sw_interval_t value){
    g_settings.switch_interval = value;
    settings_update(&g_settings);
    return;
}
int32_t get_var_brightness(){
    return g_settings.brightness;
}
void set_var_brightness(int32_t value){
    g_settings.brightness = value;
    settings_update(&g_settings);
    return;
}
const char *get_var_nand_label(){
    static char buf[128];
    snprintf(buf, sizeof(buf), 
        "%d/%dMB", 
        get_nand_available_size() / 1024 / 1024, 
        get_nand_total_size() / 1024 / 1024
    );
    return buf;
}
void set_var_nand_label(const char *value){
    return;
}
const char *get_var_sd_label(){
    static char buf[128];
    if(!is_sdcard_inserted()){
        return "SD卡不存在";
    }
    snprintf(buf, sizeof(buf), 
        "%d/%dMB", 
        get_sd_available_size() / 1024 / 1024, 
        get_sd_total_size() / 1024 / 1024
    );
    return buf;
}
void set_var_sd_label(const char *value){
    return;
}

int32_t get_var_nand_percent(){
    return (get_nand_available_size() * 100) / get_nand_total_size();
}
void set_var_nand_percent(int32_t value){
    return;
}
int32_t get_var_sd_percent(){
    if(!is_sdcard_inserted()){
        return 0;
    }
    return (get_sd_available_size() * 100) / get_sd_total_size();
}
void set_var_sd_percent(int32_t value){
    return;
}

const char *get_var_displayimg_size_lbl(){
    return "1/2";
}
void set_var_displayimg_size_lbl(const char *value){
    return;
}

void action_op_sel_cb(lv_event_t * e){
    log_debug("action_op_sel_cb");
}

extern int g_running;
void action_usb_download(lv_event_t * e){
    log_debug("action_usb_download");
    g_running = 0;
}

void action_shutdown(lv_event_t * e){
    log_debug("action_shutdown");
    system("shutdown -f");
}

static curr_screen_t g_cur_scr;

// 目前只会在主界面调用这个方法。所以只需要处理从mainmenu到oplist的动画。
void action_show_oplist(lv_event_t * e){
    log_debug("action_show_oplist");
    lv_display_t * disp = lv_display_get_default();
    lvgl_drm_warp_t* lvgl_drm_warp = (lvgl_drm_warp_t*)lv_display_get_driver_data(disp);

    layer_animation_ease_in_out_move(
        lvgl_drm_warp->layer_animation, 
        DRM_WARPPER_LAYER_UI, 
        0, 0, 
        0, UI_OPLIST_Y, 
        UI_LAYER_ANIMATION_DURATION, 0);
    loadScreen(SCREEN_ID_OPLIST);
}

void action_show_menu(lv_event_t * e){
    log_debug("action_show_menu");
    lv_display_t * disp = lv_display_get_default();
    lvgl_drm_warp_t* lvgl_drm_warp = (lvgl_drm_warp_t*)lv_display_get_driver_data(disp);

    // 对于显示位置不一样的屏幕（spinner和oplist），需要进行动画处理
    switch(g_cur_scr){
        case curr_screen_t_SCREEN_OPLIST:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, UI_OPLIST_Y, 
                0, 0, 
                UI_LAYER_ANIMATION_DURATION, 0);
            break;
        case curr_screen_t_SCREEN_SPINNER:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, SCREEN_HEIGHT, 
                0, 0, 
                UI_LAYER_ANIMATION_DURATION, 0);
            break;
        default:
            break;
    }
    loadScreen(SCREEN_ID_MAINMENU);
}
void action_format_sd_card(lv_event_t * e){
    log_debug("action_format_sd_card");
}
void action_show_sysinfo(lv_event_t * e){
    log_debug("action_show_sysinfo");
    loadScreen(SCREEN_ID_SYSINFO);
}


// 全局按钮回调。主要用于屏幕切换时放过渡。
void screen_key_event_cb(uint32_t key){
    lv_display_t* disp = lv_display_get_default();
    lvgl_drm_warp_t* lvgl_drm_warp = (lvgl_drm_warp_t *)lv_display_get_driver_data(disp);

    log_debug("screen_key_event_cb: g_cur_scr = %d, key = %d", g_cur_scr, key);
    bool is_editing = lv_group_get_editing(groups.op);

    // 展示扩列图 界面，按下3/4按钮都可关闭
    if (g_cur_scr == curr_screen_t_SCREEN_DISPLAYIMG){
        if(key == LV_KEY_ESC || key == LV_KEY_ENTER){
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation,
                DRM_WARPPER_LAYER_UI,
                0, 0,
                0, SCREEN_HEIGHT,
                UI_LAYER_ANIMATION_DURATION, 0);
            loadScreen(SCREEN_ID_SPINNER);
        }
        return;
    }

    // 其他界面
    if (g_cur_scr != curr_screen_t_SCREEN_SPINNER){
        // 不在编辑状态的时候再按下esc 即关闭设置UI.
        if(key == LV_KEY_ESC && !is_editing){
            int from_y = 0;
            if(g_cur_scr == curr_screen_t_SCREEN_OPLIST){
                from_y = UI_OPLIST_Y;
            }
            else{
                from_y = 0;
            }
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation,
                DRM_WARPPER_LAYER_UI,
                0, from_y,
                0, SCREEN_HEIGHT,
                UI_LAYER_ANIMATION_DURATION, 0);
            loadScreen(SCREEN_ID_SPINNER);
        }
        return;
    }

    // spinner（空界面），处理ui出现
    switch(key){
        // go to oplist screen
        case LV_KEY_LEFT:
        case LV_KEY_RIGHT:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, SCREEN_HEIGHT, 
                0, UI_OPLIST_Y, 
                UI_LAYER_ANIMATION_DURATION, 0);
            loadScreen(SCREEN_ID_OPLIST);
            break;
        case LV_KEY_ENTER:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, SCREEN_HEIGHT, 
                0, 0, 
                UI_LAYER_ANIMATION_DURATION, 0);
            loadScreen(SCREEN_ID_DISPLAYIMG);
            break;
        case LV_KEY_ESC:
            layer_animation_ease_in_out_move(
                lvgl_drm_warp->layer_animation, 
                DRM_WARPPER_LAYER_UI, 
                0, SCREEN_HEIGHT, 
                0, 0, 
                UI_LAYER_ANIMATION_DURATION, 0);
            loadScreen(SCREEN_ID_MAINMENU);
            break;
    }
}

void action_screen_loaded_cb(lv_event_t * e){
    g_cur_scr = (curr_screen_t)(lv_event_get_user_data(e));
    log_debug("action_screen_loaded_cb: cur_scr = %d", g_cur_scr);
};

void action_displayimg_key(lv_event_t * e){
    log_debug("action_displayimg_key");
}

// 事件有点问题 此处还没有实现，
void action_brightness_pressed(lv_event_t * e){
    log_debug("action_brightness_pressed");
    lv_display_t* disp = lv_display_get_default();
    lvgl_drm_warp_t* lvgl_drm_warp = (lvgl_drm_warp_t *)lv_display_get_driver_data(disp);
    lv_obj_t* brightness_obj = lv_event_get_target(e);
    bool is_pressed = lv_obj_has_state(brightness_obj, LV_STATE_PRESSED);

    if(is_pressed){
        layer_animation_ease_in_out_move(
            lvgl_drm_warp->layer_animation, 
            DRM_WARPPER_LAYER_UI, 
            0, 0, 
            0, UI_BRIGHTNESS_Y, 
            UI_LAYER_ANIMATION_DURATION, 0);
    }
    else{
        layer_animation_ease_in_out_move(
            lvgl_drm_warp->layer_animation, 
            DRM_WARPPER_LAYER_UI, 
            0, UI_BRIGHTNESS_Y, 
            0, 0, 
            UI_LAYER_ANIMATION_DURATION, 0);
    }
}
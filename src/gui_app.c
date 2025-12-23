#include "gui_app.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "log.h"

void gui_app_create_ui(void){
    log_info("into demo ui");
    lv_demo_benchmark();
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_TRANSP, LV_PART_MAIN);
    lv_disp_set_bg_opa(NULL, LV_OPA_TRANSP);

}
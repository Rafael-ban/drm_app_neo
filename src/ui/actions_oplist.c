// 干员信息 专用

#include <src/core/lv_obj_event.h>
#include <src/core/lv_obj_private.h>
#include <src/misc/lv_event.h>
#include <stdint.h>
#include <stdlib.h>

#include "ui.h"
#include "utils/log.h"
#include "prts/prts.h"
#include "ui/actions_oplist.h"
#include "styles.h"
#include "ui/scr_transition.h"


ui_oplist_t g_ui_oplist;
extern objects_t objects;

static void op_btn_click_cb(lv_event_t *e){
    prts_operator_entry_t* op = (prts_operator_entry_t*)lv_event_get_user_data(e);

    prts_t *prts = g_ui_oplist.prts;
    
    prts_request_set_operator(prts, op->index);
    
    ui_schedule_screen_transition(curr_screen_t_SCREEN_SPINNER);
}

// 我们这里要用EEZ 生成的
// void create_user_widget_operator_entry(lv_obj_t *parent_obj, int startWidgetIndex);
// 来创建干员列表项。但是它startWidgetIndex是相对于eez的objects的。
// 我们希望添加到自己的信息表里。因此，这里有一个非常，非常，非常，非常Dirty的hacks
static void add_prts_operator_to_ui(prts_operator_entry_t* op,ui_oplist_entry_objs_t* objs){
    log_debug("add_prts_operator_to_ui: op->operator_name = %s", op->operator_name);
    log_debug("add_prts_operator_to_ui: op->description = %s", op->description);
    log_debug("add_prts_operator_to_ui: op->icon_path = %s", op->icon_path);

    //stolen from eez generated code.
    lv_obj_t *obj = lv_obj_create(objects.oplst_container);
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, LV_PCT(97), 80);
    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 对侧的写入方法是：((lv_obj_t **)&objects)[startWidgetIndex + 0] = obj;
    // 也就是*((lv_obj_t **)&objects) + startWidgetIndex) = obj;
    // 令 startWidgetIndex = (lv_obj_t**)&objs->opbtn - (lv_obj_t **)&objects;
    // 这样实际的操作就是  *((lv_obj_t**)&objs->opbtn) = obj，即objs->opbtn = obj
    #warning "Dirty hacks happened here. If application crash during prts->ui sync, Please check alignness and such."
    int startWidgetIndex =  (lv_obj_t**)&objs->opbtn - (lv_obj_t **)&objects;
    create_user_widget_operator_entry(obj, startWidgetIndex);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    add_style_op_entry(obj);

    lv_label_set_text(objs->opname, op->operator_name);
    lv_label_set_text(objs->opdesc, op->description);
    lv_image_set_src(objs->oplogo, op->icon_path);
    lv_obj_add_event_cb(objs->opbtn, op_btn_click_cb, LV_EVENT_PRESSED, (void *)op);
}

//自己添加的方法
void ui_oplist_init(prts_t* prts){
    g_ui_oplist.prts = prts;
    g_ui_oplist.entry_count = 0;

    log_info("START prts->ui sync!! (see compiler warning above)");
    // 清空干员列表容器
    lv_obj_clean(objects.oplst_container);
    for(int i=0;i < prts->operator_count;i++){
        add_prts_operator_to_ui(&prts->operators[i], &g_ui_oplist.entry_objs[i]);
    }
    g_ui_oplist.entry_count = prts->operator_count;
}

void add_oplist_btn_to_group(){
    lv_group_remove_all_objs(groups.op);
    for(int i=0; i< g_ui_oplist.entry_count; i++){
        lv_group_add_obj(groups.op, g_ui_oplist.entry_objs[i].opbtn);
    }
    lv_group_add_obj(groups.op, objects.mainmenu_btn);
}
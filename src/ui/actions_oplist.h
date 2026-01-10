//  干员信息 专用
#pragma once

#include "prts/prts.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *opbtn;
    lv_obj_t *oplogo;
    lv_obj_t *opdesc;
    lv_obj_t *opname;
} ui_oplist_entry_objs_t;

typedef struct {
    prts_t* prts;
    ui_oplist_entry_objs_t entry_objs[PRTS_OPERATORS_MAX];
    int entry_count;
} ui_oplist_t;


// UI层就先全局变量漫天飞吧....
extern ui_oplist_t g_ui_oplist;

// 自己添加的方法
void ui_oplist_init(prts_t* prts);
void add_oplist_btn_to_group();
void ui_oplist_focus_current_operator();
// EEZ回调不需要添加。

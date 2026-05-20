#pragma once

#include "app_ui.h"
#include "esp32_s3_szp.h"

LV_FONT_DECLARE(font_alipuhui20);

extern lv_obj_t * main_obj;
extern lv_obj_t * app_obj; // 应用界面
extern lv_obj_t * main_text_label;
extern lv_obj_t * icon_in_obj;
extern int icon_flag;

extern lv_obj_t * main_date_label;
extern lv_obj_t * main_time_label;
extern lv_obj_t * clock_date_label;
extern lv_obj_t * clock_time_label;

void att_event_handler(lv_event_t * e);
void music_event_handler(lv_event_t * e);
void sdcard_event_handler(lv_event_t * e);
void camera_event_handler(lv_event_t * e);
void wifiset_event_handler(lv_event_t * e);
void btset_event_handler(lv_event_t * e);

void clock_event_handler(lv_event_t * e);
void value_update_cb(lv_timer_t * timer);
void app_time_label_create(void);
void app_time_label_clear(void);

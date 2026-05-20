#include "app_ui_internal.h"
#include <time.h>
#include <sys/time.h>

lv_obj_t *clock_date_label;
lv_obj_t *clock_time_label;

static void btn_clock_back_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        lv_obj_del(icon_in_obj);
        icon_in_obj = NULL;
        clock_date_label = NULL;
        clock_time_label = NULL;
    }
}

void clock_event_handler(lv_event_t * e)
{
    (void)e;

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_radius(&style, 10);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_color_hex(0xffffff));
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    lv_style_set_width(&style, 320);
    lv_style_set_height(&style, 240);

    icon_in_obj = lv_obj_create(lv_scr_act());
    lv_obj_add_style(icon_in_obj, &style, 0);

    lv_obj_t *clock_title = lv_obj_create(icon_in_obj);
    lv_obj_set_size(clock_title, 320, 40);
    lv_obj_set_style_pad_all(clock_title, 0, 0);
    lv_obj_align(clock_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(clock_title, lv_color_hex(0xd8b010), 0);

    lv_obj_t *clock_title_label = lv_label_create(clock_title);
    lv_label_set_text(clock_title_label, "\xE6\x97\xB6" "\xE9\x92\x9F");
    lv_obj_set_style_text_color(clock_title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(clock_title_label, &font_alipuhui20, 0);
    lv_obj_align(clock_title_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn_back = lv_btn_create(clock_title);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_style_pad_all(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, btn_clock_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(label_back, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_back, lv_color_hex(0xffffff), 0);
    lv_obj_align(label_back, LV_ALIGN_CENTER, -10, 0);

    /* Clock page labels are independent from the main page labels. */
    clock_time_label = lv_label_create(icon_in_obj);
    lv_obj_set_style_text_font(clock_time_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(clock_time_label, lv_color_hex(0x000000), 0);
    lv_obj_align(clock_time_label, LV_ALIGN_CENTER, 0, -20);

    clock_date_label = lv_label_create(icon_in_obj);
    lv_obj_set_style_text_font(clock_date_label, &font_alipuhui20, 0);
    lv_obj_set_style_text_color(clock_date_label, lv_color_hex(0x000000), 0);
    lv_obj_align(clock_date_label, LV_ALIGN_CENTER, 0, 30);

    /* Show current system time once before the WiFi timer refreshes it. */
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    lv_label_set_text_fmt(clock_time_label, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    lv_label_set_text_fmt(clock_date_label, "%d" "\xE5\xB9\xB4" "%02d" "\xE6\x9C\x88" "%02d" "\xE6\x97\xA5",
                          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
}

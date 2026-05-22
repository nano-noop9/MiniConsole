#include "app_ui_internal.h"
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

lv_obj_t *clock_date_label;
lv_obj_t *clock_time_label;
static lv_obj_t *main_weather_location_label;
static lv_obj_t *main_weather_temp_label;

static lv_obj_t *stopwatch_label; //秒表显示标签
static lv_obj_t *stopwatch_start_label; //开始暂停按钮
static lv_timer_t *stopwatch_timer; //50ms定时器标签
static uint32_t stopwatch_elapsed_ms; //记录累计时间
static uint32_t stopwatch_start_tick; //记录当前的 lv_tick_get()
static bool stopwatch_running;

#define STOPWATCH_REFRESH_MS 50


/************************************************  秒表app功能 **********************************************************/
static void stopwatch_update_label(void) //这里写出来方便其他地方调用
{
    if(stopwatch_label == NULL)
    {
        return;
    }

    uint32_t show_ms = stopwatch_elapsed_ms;
    if(stopwatch_running)
    {
        show_ms += lv_tick_elaps(stopwatch_start_tick);
    }

    uint32_t total_seconds = show_ms / 1000;
    uint32_t minutes = total_seconds / 60;
    uint32_t seconds = total_seconds % 60;
    uint32_t centiseconds = (show_ms % 1000) / 10;

    lv_label_set_text_fmt(stopwatch_label, "%02u:%02u.%02u",
                          (unsigned int)minutes,
                          (unsigned int)seconds,
                          (unsigned int)centiseconds);
}

static void stopwatch_set_start_label(void)
{
    if(stopwatch_start_label != NULL)
    {
        lv_label_set_text(stopwatch_start_label, stopwatch_running ? "停止" : "开始");
    }
}

static void stopwatch_timer_cb(lv_timer_t * timer)
{
    (void)timer;
    stopwatch_update_label();
}

static void stopwatch_stop_timer(void)
{
    if(stopwatch_timer != NULL)
    {
        lv_timer_del(stopwatch_timer);
        stopwatch_timer = NULL;
    }
}

static void btn_clock_back_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        stopwatch_stop_timer();
        lv_obj_del(icon_in_obj);
        icon_in_obj = NULL;
        // 这里只关闭秒表页面，主页面时钟标签还在，不能清空它们的指针。
        stopwatch_label = NULL;
        stopwatch_start_label = NULL;
        stopwatch_running = false;
    }
}

static void btn_reset_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        // 秒表清零后刷新显示
        stopwatch_stop_timer();
        stopwatch_running = false;
        stopwatch_elapsed_ms = 0;
        stopwatch_update_label();
        stopwatch_set_start_label();
    }
}

static void btn_start_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_CLICKED)
    {
        return;
    }

    if(stopwatch_running) //把本次运行经过的时间加入累计值，然后停止刷新定时器。
    {
        stopwatch_elapsed_ms += lv_tick_elaps(stopwatch_start_tick);
        stopwatch_running = false;
        stopwatch_stop_timer();
    }
    else //记录开始时间，标记为运行中，并创建定时刷新。
    {
        stopwatch_start_tick = lv_tick_get();
        stopwatch_running = true;
        if(stopwatch_timer == NULL)
        {
            stopwatch_timer = lv_timer_create(stopwatch_timer_cb, STOPWATCH_REFRESH_MS, NULL);
        }
    }

    stopwatch_update_label();
    stopwatch_set_start_label();
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
    lv_obj_set_style_bg_color(clock_title, lv_color_hex(0x87CEEB), 0);

    lv_obj_t *clock_title_label = lv_label_create(clock_title);
    lv_label_set_text(clock_title_label, "秒表");
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

    //秒表功能

    //秒表显示计时
    stopwatch_label = lv_label_create(icon_in_obj);
    lv_obj_set_style_text_font(stopwatch_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(stopwatch_label, lv_color_hex(0x000000), 0);
    lv_obj_align(stopwatch_label, LV_ALIGN_CENTER, 0, -20);
    stopwatch_elapsed_ms = 0;
    stopwatch_running = false;
    stopwatch_update_label();

    //秒表复位按钮
    lv_obj_t *btn_reset = lv_btn_create(icon_in_obj);
    lv_obj_set_size(btn_reset, 100, 80);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
    lv_obj_add_event_cb(btn_reset, btn_reset_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *reset_label = lv_label_create(btn_reset);
    lv_label_set_text(reset_label, "清零");
    lv_obj_set_style_text_font(reset_label, &font_alipuhui20, 0);
    lv_obj_align(reset_label, LV_ALIGN_CENTER, 0, 0);


    //秒表开始/暂停按钮
    lv_obj_t *start_btn = lv_btn_create(icon_in_obj);
    lv_obj_set_size(start_btn, 100, 80);
    lv_obj_align(start_btn, LV_ALIGN_BOTTOM_LEFT, 30, -30);
    lv_obj_add_event_cb(start_btn, btn_start_cb, LV_EVENT_CLICKED, NULL);

    stopwatch_start_label = lv_label_create(start_btn);
    lv_label_set_text(stopwatch_start_label, "开始");
    lv_obj_set_style_text_font(stopwatch_start_label, &font_alipuhui20, 0);
    lv_obj_align(stopwatch_start_label, LV_ALIGN_CENTER, 0, 0);
}


/************************************************  主页天气app功能 *********************************************************/

void weather_event_handler(lv_event_t * e)
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

    lv_obj_t *weather_title = lv_obj_create(icon_in_obj);
    lv_obj_set_size(weather_title, 320, 40);
    lv_obj_set_style_pad_all(weather_title, 0, 0);
    lv_obj_align(weather_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(weather_title, lv_color_hex(0xcd5c5c), 0);

    lv_obj_t *weather_title_label = lv_label_create(weather_title);
    lv_label_set_text(weather_title_label, "近日天气");
    lv_obj_set_style_text_color(weather_title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(weather_title_label, &font_alipuhui20, 0);
    lv_obj_align(weather_title_label, LV_ALIGN_CENTER, 0, 0);
}


/************************************************  主页天气显示 *********************************************************/

void main_weather_create(void)
{
    if(main_obj == NULL)
    {
        return;
    }

    if(main_weather_temp_label == NULL || lv_obj_is_valid(main_weather_temp_label) == false)
    {
        // 温度和单位一起使用大字体显示。
        main_weather_temp_label = lv_label_create(main_obj);
        lv_obj_set_width(main_weather_temp_label, 135);
        lv_obj_set_style_text_align(main_weather_temp_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(main_weather_temp_label, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(main_weather_temp_label, lv_color_hex(0x000000), 0);
        lv_obj_align(main_weather_temp_label, LV_ALIGN_TOP_RIGHT, -15, 50);
        lv_label_set_text(main_weather_temp_label, "--" "\xC2\xB0" "C");
    }

    if(main_weather_location_label == NULL || lv_obj_is_valid(main_weather_location_label) == false)
    {
        // 温度下方显示“省 | 城市     天气”。
        main_weather_location_label = lv_label_create(main_obj);
        lv_obj_set_width(main_weather_location_label, 180);
        lv_obj_set_style_text_align(main_weather_location_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(main_weather_location_label, &font_alipuhui20, 0);
        lv_obj_set_style_text_color(main_weather_location_label, lv_color_hex(0x000000), 0);
        lv_obj_align(main_weather_location_label, LV_ALIGN_TOP_RIGHT, -10, 102);
        lv_label_set_text(main_weather_location_label, "-- | --   获取中");
    }
}

void main_weather_update(const char *province, const char *city, const char *weather, const char *temperature)
{
    lvgl_port_lock(0);
    main_weather_create();

    if(main_weather_location_label != NULL && main_weather_temp_label != NULL)
    {

        lv_label_set_text_fmt(main_weather_location_label, "%s | %s   %s", province, city, weather);
        lv_label_set_text_fmt(main_weather_temp_label, "%s" "\xC2\xB0" "C", temperature);
    }

    lvgl_port_unlock();
}

/************************************************  主页时钟显示 **********************************************************/
void main_clock_create(void)
{
    /* Clock page labels are independent from the main page labels. */
    clock_time_label = lv_label_create(main_obj);
    lv_obj_set_style_text_font(clock_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(clock_time_label, lv_color_hex(0x000000), 0);
    lv_obj_align(clock_time_label, LV_ALIGN_TOP_LEFT, 10, 50);

    clock_date_label = lv_label_create(main_obj);
    lv_obj_set_style_text_font(clock_date_label, &font_alipuhui20, 0);
    lv_obj_set_style_text_color(clock_date_label, lv_color_hex(0x000000), 0);
    lv_obj_align_to(clock_date_label,clock_time_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    /* Show current system time once before the WiFi timer refreshes it. */
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    lv_label_set_text_fmt(clock_time_label, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    lv_label_set_text_fmt(clock_date_label, "%d" "\xE5\xB9\xB4" "%02d" "\xE6\x9C\x88" "%02d" "\xE6\x97\xA5",
                          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

    // 主页面创建后立即刷新时间，未联网时也让1970时间继续走动。
    time_update_timer_start();
}

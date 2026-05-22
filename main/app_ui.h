#pragma once


/*********************** 开机界面 ****************************/
// 开机界面
void lv_gui_start(void);


/*********************** 主界面 ****************************/
void lv_main_page(void);
void main_weather_create(void);
void main_weather_update(const char *province, const char *city, const char *weather, const char *temperature);

#define WEATHER_DAILY_MAX_DAYS 3

typedef struct {
    char date[11];
    char text_day[24];
    char text_night[24];
    char high[8];
    char low[8];
    char rainfall[8];
    char humidity[8];
} weather_daily_info_t;

void main_weather_daily_update(const weather_daily_info_t daily[], int count);


/*********************** 音乐播放器 ****************************/
void mp3_player_init(void);
void music_ui(void);
void music_play_pause_toggle(void);

void ai_gui_in(void);
void ai_gui_out(void);

void ai_play(void);
void ai_pause(void);
void ai_resume(void);
void ai_prev_music(void);
void ai_next_music(void);
void ai_volume_up(void);
void ai_volume_down(void);



void ai_open_icon1(void);
void ai_open_icon2(void);
void ai_open_icon3(void);
void ai_open_icon4(void);
void ai_open_icon5(void);
void ai_open_icon6(void);
void ai_tuichu(void);


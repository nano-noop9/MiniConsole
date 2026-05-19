#include "app_ui_internal.h"
#include "audio_player.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "app_ui";

/*********************  第2个图标  音乐播放器  *********************************************************************************************/
#define MUSIC_MAX_FILES     32
#define MUSIC_NAME_SIZE     256
#define MUSIC_PATH_SIZE     512

static audio_player_config_t player_config = {0};
static uint8_t g_sys_volume = VOLUME_DEFAULT;
static char music_names[MUSIC_MAX_FILES][MUSIC_NAME_SIZE];
static char music_paths[MUSIC_MAX_FILES][MUSIC_PATH_SIZE];
static size_t music_file_count = 0;
static size_t music_file_index = 0;
static bool music_sd_mounted = false;
static bool music_player_ready = false;

lv_obj_t *music_list = NULL;
lv_obj_t *label_play_pause = NULL;
lv_obj_t *btn_play_pause = NULL;
lv_obj_t *volume_slider = NULL;

lv_obj_t *music_title_label = NULL;
lv_obj_t *btn_music_back = NULL;

static void music_set_play_button_state(bool playing);

static bool music_is_mp3_file(const char *name)
{
    const char *extension = strrchr(name, '.');
    if(extension == NULL)
    {
        return false;
    }

    if((extension[1] == 'm' || extension[1] == 'M') &&
       (extension[2] == 'p' || extension[2] == 'P') &&
       extension[3] == '3' &&
       extension[4] == '\0')
    {
        return true;
    }

    return false;
}

static void music_clear_files(void)
{
    music_file_count = 0;
    music_file_index = 0;
}

static esp_err_t music_scan_sdcard(void)
{
    DIR *dir = opendir(SD_MOUNT_POINT);
    if(dir == NULL)
    {
        return ESP_FAIL;
    }

    music_clear_files();

    struct dirent *ent;
    while((ent = readdir(dir)) != NULL)
    {
        if(ent->d_type == DT_DIR)
        {
            continue;
        }

        if(!music_is_mp3_file(ent->d_name))
        {
            continue;
        }

        if(music_file_count >= MUSIC_MAX_FILES)
        {
            break;
        }

        snprintf(music_names[music_file_count], sizeof(music_names[music_file_count]), "%s", ent->d_name);
        snprintf(music_paths[music_file_count], sizeof(music_paths[music_file_count]), "%s/%s", SD_MOUNT_POINT, ent->d_name);
        music_file_count++;
    }

    closedir(dir);
    return ESP_OK;
}

// 播放指定序号的音乐
static void play_index(size_t index)
{
    if(!music_player_ready || music_file_count == 0 || index >= music_file_count)
    {
        return;
    }

    ESP_LOGI(TAG, "play_index(%d)", (int)index);

    FILE *fp = fopen(music_paths[index], "rb");
    if(fp)
    {
        esp_err_t ret = ESP_OK;
        ESP_LOGI(TAG, "Playing '%s'", music_paths[index]);
        music_file_index = index;
        ret = audio_player_play(fp);
        if(ret != ESP_OK)
        {
            ESP_LOGE(TAG, "audio_player_play failed: %d", ret);
            fclose(fp);
        }
    }
    else
    {
        ESP_LOGE(TAG, "unable to open index %d, filename '%s'", (int)index, music_paths[index]);
    }
}

// 设置声音处理函数
static esp_err_t _audio_player_mute_fn(AUDIO_PLAYER_MUTE_SETTING setting)
{
    esp_err_t ret = ESP_OK;

    bsp_codec_mute_set(setting == AUDIO_PLAYER_MUTE ? true : false);
    if(setting == AUDIO_PLAYER_UNMUTE)
    {
        bsp_codec_volume_set(g_sys_volume, NULL);
    }
    ret = ESP_OK;

    return ret;
}

// 播放音乐函数
static esp_err_t _audio_player_write_fn(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;

    ret = bsp_i2s_write(audio_buffer, len, bytes_written, timeout_ms);

    return ret;
}

// 设置采样率
static esp_err_t _audio_player_std_clock(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;

    ret = bsp_codec_set_fs(rate, bits_cfg, ch);
    return ret;
}

// 回调函数
static void _audio_player_callback(audio_player_cb_ctx_t *ctx)
{
    ESP_LOGI(TAG, "ctx->audio_event = %d", ctx->audio_event);
    switch(ctx->audio_event)
    {
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE:
        if(music_file_count > 0)
        {
            music_file_index++;
            if(music_file_index >= music_file_count)
            {
                music_file_index = 0;
            }

            ESP_LOGI(TAG, "playing index '%d'", (int)music_file_index);
            play_index(music_file_index);
            if(music_list != NULL)
            {
                lvgl_port_lock(0);
                lv_dropdown_set_selected(music_list, music_file_index);
                lvgl_port_unlock();
            }
        }
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
        ESP_LOGI(TAG, "AUDIO_PLAYER_REQUEST_PLAY");
        pa_en(1);
        if(label_play_pause != NULL || btn_play_pause != NULL)
        {
            lvgl_port_lock(0);
            music_set_play_button_state(true);
            lvgl_port_unlock();
        }
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
        ESP_LOGI(TAG, "AUDIO_PLAYER_REQUEST_PAUSE");
        pa_en(0);
        if(label_play_pause != NULL || btn_play_pause != NULL)
        {
            lvgl_port_lock(0);
            music_set_play_button_state(false);
            lvgl_port_unlock();
        }
        break;
    default:
        break;
    }
}

// mp3播放器初始化
static esp_err_t music_player_init(void)
{
    player_config.mute_fn = _audio_player_mute_fn;
    player_config.write_fn = _audio_player_write_fn;
    player_config.clk_set_fn = _audio_player_std_clock;
    player_config.priority = 6;
    player_config.coreID = 1;

    esp_err_t ret = audio_player_new(player_config);
    if(ret != ESP_OK)
    {
        return ret;
    }

    ret = audio_player_callback_register(_audio_player_callback, NULL);
    if(ret != ESP_OK)
    {
        audio_player_delete();
        return ret;
    }

    music_player_ready = true;
    return ret;
}

// 按钮样式相关定义
typedef struct {
    lv_style_t style_bg;
    lv_style_t style_play_bg;
    lv_style_t style_pause_bg;
    lv_style_t style_focus_no_outline;
} button_style_t;

static button_style_t g_btn_styles;

button_style_t *ui_button_styles(void)
{
    return &g_btn_styles;
}

static void music_set_play_button_state(bool playing)
{
    if(label_play_pause != NULL)
    {
        lv_label_set_text_static(label_play_pause, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }

    if(btn_play_pause != NULL)
    {
        lv_obj_clear_state(btn_play_pause, LV_STATE_CHECKED);
        lv_obj_remove_style(btn_play_pause, &ui_button_styles()->style_bg, LV_PART_MAIN);
        lv_obj_remove_style(btn_play_pause, &ui_button_styles()->style_play_bg, LV_PART_MAIN);
        lv_obj_remove_style(btn_play_pause, &ui_button_styles()->style_pause_bg, LV_PART_MAIN);
        if(playing)
        {
            lv_obj_add_style(btn_play_pause, &ui_button_styles()->style_pause_bg, LV_PART_MAIN);
        }
        else
        {
            lv_obj_add_style(btn_play_pause, &ui_button_styles()->style_play_bg, LV_PART_MAIN);
        }
    }
}

// 按钮样式初始化
static void ui_button_style_init(void)
{
    /*Init the style for the default state*/
    lv_style_init(&g_btn_styles.style_focus_no_outline);
    lv_style_set_outline_width(&g_btn_styles.style_focus_no_outline, 0);

    lv_style_init(&g_btn_styles.style_bg);
    lv_style_set_bg_opa(&g_btn_styles.style_bg, LV_OPA_100);
    lv_style_set_bg_color(&g_btn_styles.style_bg, lv_color_make(255, 255, 255));
    lv_style_set_shadow_width(&g_btn_styles.style_bg, 0);

    lv_style_init(&g_btn_styles.style_play_bg);
    lv_style_set_bg_opa(&g_btn_styles.style_play_bg, LV_OPA_100);
    lv_style_set_bg_color(&g_btn_styles.style_play_bg, lv_color_hex(0x3080ff));
    lv_style_set_shadow_width(&g_btn_styles.style_play_bg, 0); //阴影宽度为 0

    lv_style_init(&g_btn_styles.style_pause_bg);
    lv_style_set_bg_opa(&g_btn_styles.style_pause_bg, LV_OPA_100);
    lv_style_set_bg_color(&g_btn_styles.style_pause_bg, lv_color_hex(0xf87c30));
}

// 播放暂停按钮事件处理函数
static void btn_play_pause_cb(lv_event_t *event)
{
    music_play_pause_toggle();
}

// 上一首 下一首按键事件处理函数
static void btn_prev_next_cb(lv_event_t *event)
{
    if(!music_player_ready || music_file_count == 0)
    {
        return;
    }

    bool is_next = (bool) event->user_data;

    if(is_next)
    {
        ESP_LOGI(TAG, "btn next");
        music_file_index++;
        if(music_file_index >= music_file_count)
        {
            music_file_index = 0;
        }
    }
    else
    {
        ESP_LOGI(TAG, "btn prev");
        if(music_file_index == 0)
        {
            music_file_index = music_file_count - 1;
        }
        else
        {
            music_file_index--;
        }
    }

    lvgl_port_lock(0);
    lv_dropdown_set_selected(music_list, music_file_index);
    lvgl_port_unlock();

    audio_player_state_t state = audio_player_get_state();
    printf("prev_next_state=%d\n", state);
    if(state == AUDIO_PLAYER_STATE_IDLE)
    {
        // Nothing to do
    }
    else if(state == AUDIO_PLAYER_STATE_PAUSE)
    {
        ESP_LOGI(TAG, "playing index '%d'", (int)music_file_index);
        play_index(music_file_index);
        audio_player_pause();
    }
    else if(state == AUDIO_PLAYER_STATE_PLAYING)
    {
        ESP_LOGI(TAG, "playing index '%d'", (int)music_file_index);
        play_index(music_file_index);
    }
}

// 音量调节滑动条事件处理函数
static void volume_slider_cb(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_target(event);
    int volume = lv_slider_get_value(slider);
    bsp_codec_volume_set(volume, NULL);
    g_sys_volume = volume;
    ESP_LOGI(TAG, "volume '%d'", volume);
}

// 音乐列表点击事件处理函数
static void music_list_cb(lv_event_t *event)
{
    if(!music_player_ready || music_file_count == 0)
    {
        return;
    }

    music_file_index = lv_dropdown_get_selected(music_list);
    if(music_file_index >= music_file_count)
    {
        music_file_index = 0;
    }
    ESP_LOGI(TAG, "switching index to '%d'", (int)music_file_index);

    audio_player_state_t state = audio_player_get_state();
    if(state == AUDIO_PLAYER_STATE_IDLE)
    {
        play_index(music_file_index);
    }
    else if(state == AUDIO_PLAYER_STATE_PAUSE)
    {
        play_index(music_file_index);
        audio_player_pause();
    }
    else if(state == AUDIO_PLAYER_STATE_PLAYING)
    {
        play_index(music_file_index);
    }
}

static void music_dropdown_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *dropdown = lv_event_get_target(event);

    if(code == LV_EVENT_READY)
    {
        lv_obj_t *list = lv_dropdown_get_list(dropdown);
        if(list != NULL)
        {
            lv_obj_set_style_text_font(list, &font_alipuhui20, LV_PART_MAIN);
            lv_obj_set_style_text_font(list, &font_alipuhui20, LV_PART_SELECTED);

            lv_obj_set_style_text_align(list, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
            lv_obj_set_style_pad_left(list, 8, LV_PART_MAIN);
            lv_obj_set_style_pad_right(list, 8, LV_PART_MAIN);

        }
    }
}

// 音乐名称加入列表
static void build_file_list(lv_obj_t *music_list)
{
    lv_dropdown_clear_options(music_list);

    if(music_file_count == 0)
    {
        lv_dropdown_set_options_static(music_list, "未找到MP3文件");
        return;
    }

    for(size_t i = 0; i < music_file_count; i++)
    {
        lv_dropdown_add_option(music_list, music_names[i], i);
    }
    if(music_file_index >= music_file_count)
    {
        music_file_index = 0;
    }
    lv_dropdown_set_selected(music_list, music_file_index);
}

void music_play_pause_toggle(void)
{
    if(!music_player_ready || music_file_count == 0)
    {
        return;
    }

    audio_player_state_t state = audio_player_get_state();
    printf("state=%d\n", state);
    if(state == AUDIO_PLAYER_STATE_IDLE)
    {
        ESP_LOGI(TAG, "playing index '%d'", (int)music_file_index);
        play_index(music_file_index);
    }
    else if(state == AUDIO_PLAYER_STATE_PAUSE)
    {
        audio_player_resume();
    }
    else if(state == AUDIO_PLAYER_STATE_PLAYING)
    {
        audio_player_pause();
    }
}

// 播放器界面初始化
void music_ui(void)
{
    lvgl_port_lock(0);

    ui_button_style_init();
    /* 创建播放暂停控制按键 */
    btn_play_pause = lv_btn_create(icon_in_obj);
    lv_obj_align(btn_play_pause, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_size(btn_play_pause, 50, 50);
    lv_obj_set_style_radius(btn_play_pause, 25, LV_STATE_DEFAULT);
    lv_obj_clear_flag(btn_play_pause, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_add_style(btn_play_pause, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_play_pause, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUSED);

    label_play_pause = lv_label_create(btn_play_pause);

    lv_label_set_text_static(label_play_pause, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(label_play_pause, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_center(label_play_pause);

    lv_obj_set_user_data(btn_play_pause, (void *) label_play_pause);
    lv_obj_add_event_cb(btn_play_pause, btn_play_pause_cb, LV_EVENT_CLICKED, NULL);
    music_set_play_button_state(false);

    /* 创建上一首控制按键 */
    lv_obj_t *btn_play_prev = lv_btn_create(icon_in_obj);
    lv_obj_set_size(btn_play_prev, 50, 50);
    lv_obj_set_style_radius(btn_play_prev, 25, LV_STATE_DEFAULT);
    lv_obj_clear_flag(btn_play_prev, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_align_to(btn_play_prev, btn_play_pause, LV_ALIGN_OUT_LEFT_MID, -40, 0);

    lv_obj_add_style(btn_play_prev, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_play_prev, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_play_prev, &ui_button_styles()->style_bg, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_play_prev, &ui_button_styles()->style_bg, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_play_prev, &ui_button_styles()->style_bg, LV_STATE_DEFAULT);

    lv_obj_t *label_prev = lv_label_create(btn_play_prev);
    lv_label_set_text_static(label_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(label_prev, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_prev, lv_color_make(0, 0, 0), LV_STATE_DEFAULT);
    lv_obj_center(label_prev);
    lv_obj_set_user_data(btn_play_prev, (void *) label_prev);
    lv_obj_add_event_cb(btn_play_prev, btn_prev_next_cb, LV_EVENT_CLICKED, (void *) false);

    /* 创建下一首控制按键 */
    lv_obj_t *btn_play_next = lv_btn_create(icon_in_obj);
    lv_obj_set_size(btn_play_next, 50, 50);
    lv_obj_set_style_radius(btn_play_next, 25, LV_STATE_DEFAULT);
    lv_obj_clear_flag(btn_play_next, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_align_to(btn_play_next, btn_play_pause, LV_ALIGN_OUT_RIGHT_MID, 40, 0);

    lv_obj_add_style(btn_play_next, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_play_next, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_play_next, &ui_button_styles()->style_bg, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_play_next, &ui_button_styles()->style_bg, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_play_next, &ui_button_styles()->style_bg, LV_STATE_DEFAULT);

    lv_obj_t *label_next = lv_label_create(btn_play_next);
    lv_label_set_text_static(label_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(label_next, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_next, lv_color_make(0, 0, 0), LV_STATE_DEFAULT);
    lv_obj_center(label_next);
    lv_obj_set_user_data(btn_play_next, (void *) label_next);
    lv_obj_add_event_cb(btn_play_next, btn_prev_next_cb, LV_EVENT_CLICKED, (void *) true);

    /* 创建声音调节滑动条 */
    volume_slider = lv_slider_create(icon_in_obj);
    lv_obj_set_size(volume_slider, 200, 10);
    lv_obj_set_ext_click_area(volume_slider, 15);
    lv_obj_align(volume_slider, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, g_sys_volume, LV_ANIM_ON);
    lv_obj_add_event_cb(volume_slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *lab_vol_min = lv_label_create(icon_in_obj);
    lv_label_set_text_static(lab_vol_min, LV_SYMBOL_VOLUME_MID);
    lv_obj_set_style_text_font(lab_vol_min, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_vol_min, volume_slider, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    lv_obj_t *lab_vol_max = lv_label_create(icon_in_obj);
    lv_label_set_text_static(lab_vol_max, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(lab_vol_max, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_vol_max, volume_slider, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    /* 创建音乐列表 */
    music_list = lv_dropdown_create(icon_in_obj);
    lv_dropdown_clear_options(music_list);
    lv_dropdown_set_options_static(music_list, "扫描中...");

    //lv_obj_set_style_align(music_list, LV_ALIGN_LEFT_MID, LV_PART_MAIN);

    // 设置下拉列表的左右内边距，以增加选项之间的间距
    lv_obj_set_style_pad_left(music_list, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(music_list, 8, LV_PART_MAIN);

    lv_dropdown_set_symbol(music_list, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_opa(music_list, LV_OPA_TRANSP, LV_PART_INDICATOR); // 隐藏下拉图标

    lv_obj_set_style_text_font(music_list, &font_alipuhui20, LV_STATE_ANY);
    lv_obj_set_style_text_font(music_list, &font_alipuhui20, LV_PART_MAIN);
    lv_obj_set_style_text_font(music_list, &font_alipuhui20, LV_PART_SELECTED);
    lv_obj_set_style_text_font(music_list, &lv_font_montserrat_20, LV_PART_INDICATOR);
    lv_obj_set_width(music_list, 200);
    lv_obj_align(music_list, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_add_event_cb(music_list, music_dropdown_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(music_list, music_list_cb, LV_EVENT_VALUE_CHANGED, NULL);

    build_file_list(music_list);
    if(music_player_ready)
    {
        audio_player_state_t state = audio_player_get_state();
        music_set_play_button_state(state == AUDIO_PLAYER_STATE_PLAYING);
    }
    else
    {
        music_set_play_button_state(false);
    }

    lvgl_port_unlock();
}

// 返回主界面按钮事件处理函数
static void btn_music_back_cb(lv_event_t * e)
{
    lv_obj_del(icon_in_obj);
    icon_in_obj = NULL;
    music_list = NULL;
    label_play_pause = NULL;
    btn_play_pause = NULL;
    volume_slider = NULL;
    music_title_label = NULL;
    btn_music_back = NULL;
    icon_flag = 0;
}

// 进入音乐播放应用
void music_event_handler(lv_event_t * e)
{
    // 创建一个界面对像
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_radius(&style, 10);
    lv_style_set_bg_opa( &style, LV_OPA_COVER );
    lv_style_set_bg_color(&style, lv_color_hex(0xffffff));
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    lv_style_set_width(&style, 320);
    lv_style_set_height(&style, 240);

    icon_in_obj = lv_obj_create(lv_scr_act());
    lv_obj_add_style(icon_in_obj, &style, 0);

    // 创建标题背景
    lv_obj_t *music_title = lv_obj_create(icon_in_obj);
    lv_obj_set_size(music_title, 320, 40);
    lv_obj_set_style_pad_all(music_title, 0, 0);
    lv_obj_align(music_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(music_title, lv_color_hex(0xf87c30), 0);
    // 显示标题
    music_title_label = lv_label_create(music_title);
    lv_label_set_text(music_title_label, "音乐播放器");
    lv_obj_set_style_text_color(music_title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(music_title_label, &font_alipuhui20, 0);
    lv_obj_align(music_title_label, LV_ALIGN_CENTER, 0, 0);
    // 创建后退按钮
    btn_music_back = lv_btn_create(music_title);
    lv_obj_align(btn_music_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_size(btn_music_back, 60, 30);
    lv_obj_set_style_border_width(btn_music_back, 0, 0);
    lv_obj_set_style_pad_all(btn_music_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_music_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn_music_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_music_back, btn_music_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_back = lv_label_create(btn_music_back);
    lv_label_set_text(label_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(label_back, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_back, lv_color_hex(0xffffff), 0);
    lv_obj_align(label_back, LV_ALIGN_CENTER, -10, 0);

    icon_flag = 2;

    if(music_player_ready)
    {
        music_ui();
        return;
    }

    esp_err_t ret = ESP_OK;
    if(!music_sd_mounted)
    {
        ret = bsp_sdcard_mount();
        if(ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
            lv_label_set_text(music_title_label, "SD卡挂载不成功");
            music_clear_files();
            music_ui();
            return;
        }

        music_sd_mounted = true;
    }
    ret = music_scan_sdcard();
    if(ret != ESP_OK)
    {
        lv_label_set_text(music_title_label, "SD卡挂载不成功");
        music_clear_files();
        music_ui();
        return;
    }

    if(music_file_count == 0)
    {
        lv_label_set_text(music_title_label, "未找到MP3文件");
        music_ui();
        return;
    }

    ret = music_player_init();
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init audio player.");
        lv_label_set_text(music_title_label, "播放器初始化失败");
    }

    music_ui();
}

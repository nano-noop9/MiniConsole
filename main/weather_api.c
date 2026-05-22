#include "weather_api.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "app_ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//具体纬经度(这个显示乐清)
//https://api.seniverse.com/v3/weather/now.json?key=SmHzaoxeqJTS03qtf&location=28.02:120.81&language=zh-Hans&unit=c

#define WEATHER_NOW_URL "https://api.seniverse.com/v3/weather/now.json?key=SmHzaoxeqJTS03qtf&location=wenzhou&language=zh-Hans&unit=c"
#define WEATHER_DAILY_URL "https://api.seniverse.com/v3/weather/daily.json?key=SmHzaoxeqJTS03qtf&location=wenzhou&language=zh-Hans&unit=c&start=0&days=3"
#define WEATHER_HTTP_OUTPUT_BUFFER 2048

static const char *TAG = "weather_api";

typedef struct {
    char *buffer;
    int buffer_size;
    int data_len;
} weather_http_data_t;

static esp_err_t weather_http_event_handler(esp_http_client_event_t *evt)
{
    weather_http_data_t *http_data = (weather_http_data_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA && http_data != NULL) {
        int copy_len = http_data->buffer_size - http_data->data_len - 1;
        if (copy_len > evt->data_len) {
            copy_len = evt->data_len;
        }

        if (copy_len > 0) {
            memcpy(http_data->buffer + http_data->data_len, evt->data, copy_len);
            http_data->data_len += copy_len;
            http_data->buffer[http_data->data_len] = '\0';
        }
    }

    return ESP_OK;
}

static esp_err_t weather_http_get(const char *url, char *response_buffer, int response_buffer_size)
{
    weather_http_data_t http_data = {
        .buffer = response_buffer,
        .buffer_size = response_buffer_size,
        .data_len = 0,
    };

    memset(response_buffer, 0, response_buffer_size);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = weather_http_event_handler,
        .user_data = &http_data,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 8000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "天气HTTP客户端初始化失败");
        return ESP_FAIL;
    }

    // 禁止压缩，便于开发板直接解析返回的JSON文本。
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "天气HTTP状态码 = %d, content_length = %" PRId64,
                 status_code,
                 esp_http_client_get_content_length(client));

        if (status_code != 200) {
            ESP_LOGE(TAG, "天气请求返回异常状态码: %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "天气HTTP请求失败: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// 从 path 字段中提取省份：例如“温州,温州,浙江,中国”中取出“浙江”。
static bool weather_get_province_from_path(const char *path, char *province, size_t province_size)
{
    const char *country_comma = strrchr(path, ',');
    if (country_comma == NULL) {
        return false;
    }

    const char *province_end = country_comma;
    const char *province_start = province_end;
    while (province_start > path && *(province_start - 1) != ',') {
        province_start--;
    }

    size_t province_len = province_end - province_start;
    if (province_len == 0 || province_len >= province_size) {
        return false;
    }

    memcpy(province, province_start, province_len);
    province[province_len] = '\0';
    return true;
}

static void weather_now_parse_and_print(const char *json_text)
{
    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL) {
        ESP_LOGE(TAG, "实时天气JSON解析失败");
        return;
    }

    cJSON *results = cJSON_GetObjectItem(root, "results");
    cJSON *first_result = cJSON_GetArrayItem(results, 0);
    cJSON *location = cJSON_GetObjectItem(first_result, "location");
    cJSON *now = cJSON_GetObjectItem(first_result, "now");
    cJSON *city = cJSON_GetObjectItem(location, "name");
    cJSON *path = cJSON_GetObjectItem(location, "path");
    cJSON *weather_text = cJSON_GetObjectItem(now, "text");
    cJSON *temperature = cJSON_GetObjectItem(now, "temperature");
    cJSON *last_update = cJSON_GetObjectItem(first_result, "last_update");

    if (!cJSON_IsString(city) || !cJSON_IsString(weather_text) ||
        !cJSON_IsString(temperature) || !cJSON_IsString(last_update) || !cJSON_IsString(path)) {
        ESP_LOGE(TAG, "天气JSON字段不完整");
        cJSON_Delete(root);
        return;
    }

    char province_buf[16] = "";
    if (weather_get_province_from_path(path->valuestring, province_buf, sizeof(province_buf)) == false) {
        ESP_LOGE(TAG, "省份字段解析失败");
        cJSON_Delete(root);
        return;
    }

    ESP_LOGI(TAG, "省份: %s", province_buf);
    ESP_LOGI(TAG, "城市: %s", city->valuestring);
    ESP_LOGI(TAG, "天气: %s", weather_text->valuestring);
    ESP_LOGI(TAG, "温度: %s C", temperature->valuestring);
    ESP_LOGI(TAG, "更新时间: %s", last_update->valuestring);
    // 更新天气UI信息
    main_weather_update(province_buf, city->valuestring, weather_text->valuestring, temperature->valuestring);

    cJSON_Delete(root);
}

static void weather_copy_string(char *dst, size_t dst_size, const char *src)
{
    if(dst_size == 0)
    {
        return;
    }

    if(src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

/*
 * 最近三天天气预报解析代码。。
 * 学习时可以对照 weather_now_parse_and_print()：
 * 先解析根节点，再进入 results[0].daily[0]，最后取 high、low 等字段。
 */
static void weather_daily_parse_and_update(const char *json_text)
{
    cJSON *root = cJSON_Parse(json_text);
    if(root == NULL) {
        ESP_LOGE(TAG, "天气预报JSON解析失败");
        return;
    }

    cJSON *results = cJSON_GetObjectItem(root, "results");
    cJSON *first_result = cJSON_GetArrayItem(results, 0);
    cJSON *daily = cJSON_GetObjectItem(first_result, "daily");

    if(!cJSON_IsArray(daily)) {
        ESP_LOGE(TAG, "天气预报JSON字段不完整");
        cJSON_Delete(root);
        return;
    }

    weather_daily_info_t daily_info[WEATHER_DAILY_MAX_DAYS] = {0};
    int daily_count = cJSON_GetArraySize(daily);
    if(daily_count > WEATHER_DAILY_MAX_DAYS) {
        daily_count = WEATHER_DAILY_MAX_DAYS;
    }

    int valid_count = 0;
    for(int i = 0; i < daily_count; i++) {
        cJSON *day = cJSON_GetArrayItem(daily, i);
        cJSON *date = cJSON_GetObjectItem(day, "date");
        cJSON *text_day = cJSON_GetObjectItem(day, "text_day");
        cJSON *text_night = cJSON_GetObjectItem(day, "text_night");
        cJSON *high = cJSON_GetObjectItem(day, "high");
        cJSON *low = cJSON_GetObjectItem(day, "low");
        cJSON *rainfall = cJSON_GetObjectItem(day, "rainfall");
        cJSON *humidity = cJSON_GetObjectItem(day, "humidity");

        if(!cJSON_IsString(date) || !cJSON_IsString(text_day) ||
           !cJSON_IsString(text_night) || !cJSON_IsString(high) ||
           !cJSON_IsString(low) || !cJSON_IsString(rainfall) ||
           !cJSON_IsString(humidity)) {
            ESP_LOGE(TAG, "第%d天天气预报字段不完整", i + 1);
            continue;
        }

        weather_daily_info_t *info = &daily_info[valid_count];
        weather_copy_string(info->date, sizeof(info->date), date->valuestring);
        weather_copy_string(info->text_day, sizeof(info->text_day), text_day->valuestring);
        weather_copy_string(info->text_night, sizeof(info->text_night), text_night->valuestring);
        weather_copy_string(info->high, sizeof(info->high), high->valuestring);
        weather_copy_string(info->low, sizeof(info->low), low->valuestring);
        snprintf(info->rainfall, sizeof(info->rainfall), "%.1f", atof(rainfall->valuestring));
        weather_copy_string(info->humidity, sizeof(info->humidity), humidity->valuestring);

        ESP_LOGI(TAG, "预报%d: %s %s-%s %s~%s C 降雨%s 湿度%s%%",
                 i + 1,
                 info->date,
                 info->text_day,
                 info->text_night,
                 info->low,
                 info->high,
                 info->rainfall,
                 info->humidity);
        valid_count++;
    }

    if(valid_count > 0) {
        main_weather_daily_update(daily_info, valid_count);
    }

    cJSON_Delete(root);
}


#define WEATHER_AUTO_UPDATE_ENABLE 0
#define WEATHER_UPDATE_INTERVAL_MS (10 * 60 * 1000)

   /*
     * 最近三天天气预报先不显示，代码保留给后面学习。
     * 按照上面 weather_now_parse_and_print() 的方法：
     * 1. 先用 cJSON_Parse() 把字符串转成 JSON 根节点。
     * 2. 再用 cJSON_GetObjectItem() / cJSON_GetArrayItem() 一层层取 results[0].daily[0]。
     * 3. 最后用 cJSON_IsString() 判断字段有效，再读取 valuestring。
     */
    // if (weather_http_get(WEATHER_DAILY_URL, response_buffer, sizeof(response_buffer)) == ESP_OK) {
    //     weather_daily_parse_and_print(response_buffer);
    // }

static void weather_fetch_task(void *pvParameters)
{
    (void)pvParameters;

    char response_buffer[WEATHER_HTTP_OUTPUT_BUFFER] = {0};

#if WEATHER_AUTO_UPDATE_ENABLE
    while (true) {
#endif
        if (weather_http_get(WEATHER_NOW_URL, response_buffer, sizeof(response_buffer)) == ESP_OK) {
            weather_now_parse_and_print(response_buffer);
        }

        if (weather_http_get(WEATHER_DAILY_URL, response_buffer, sizeof(response_buffer)) == ESP_OK) {
            weather_daily_parse_and_update(response_buffer);
        }

#if WEATHER_AUTO_UPDATE_ENABLE
        vTaskDelay(pdMS_TO_TICKS(WEATHER_UPDATE_INTERVAL_MS));
    }
#else
    vTaskDelete(NULL);
#endif
}


//如果启用了刷新天气，想防止多次创建任务，可以这样
#if WEATHER_AUTO_UPDATE_ENABLE
static TaskHandle_t weather_task_handle = NULL;
#endif

void weather_fetch_start(void)
{
#if WEATHER_AUTO_UPDATE_ENABLE
    if(weather_task_handle != NULL)
    {
        return;
    }

    xTaskCreatePinnedToCore(weather_fetch_task, "weather_fetch_task", 8 * 1024, NULL, 5, &weather_task_handle, 0);
#else
    xTaskCreatePinnedToCore(weather_fetch_task, "weather_fetch_task", 8 * 1024, NULL, 5, NULL, 0);
#endif
}

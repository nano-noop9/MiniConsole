#include "app_ui_internal.h"
#include "bt/ble_hidd_demo.h"
#include <dirent.h>
#include <sys/stat.h>

static const char *TAG = "app_ui";

/******************************** 第1个图标 姿态传感器 应用程序*************************************************************************************/
lv_obj_t * label_x; // x角度值
lv_obj_t * label_y; // y角度值
lv_obj_t * label_z; // z角度值
lv_obj_t * x_bar;   // x角度bar
lv_obj_t * y_bar;   // y角度bar
lv_obj_t * z_bar;   // z角度bar

lv_obj_t * att_label; // 标题栏文字

lv_timer_t * my_lv_timer;

lv_obj_t *btn_att_back; // att姿态应用 后退按钮

// 返回主界面按钮事件处理函数
static void btn_att_back_cb(lv_event_t * e)
{
    lv_timer_del(my_lv_timer);
    qmi8658_close(); // 关闭芯片运行
    lv_obj_del(icon_in_obj); // 删除画布
    icon_flag = 0;
}

// 定时更新姿态角度值
void att_update_cb(lv_timer_t * timer)
{
    t_sQMI8658 QMI8658;
    int att_x, att_y, att_z;
    // 获取XYZ角度
    qmi8658_fetch_angleFromAcc(&QMI8658);
    att_x = round(QMI8658.AngleX);  // 四舍五入
    att_y = round(QMI8658.AngleY);  // 四舍五入
    att_z = round(QMI8658.AngleZ);  // 四舍五入
    // 更新角度值
    lv_label_set_text_fmt(label_x, "X: %d", att_x);
    lv_label_set_text_fmt(label_y, "Y: %d", att_y);
    lv_label_set_text_fmt(label_z, "Z: %d", att_z);
    // 更新角度bar
    lv_bar_set_start_value(x_bar, att_x-10, LV_ANIM_OFF);
    lv_bar_set_value(x_bar, att_x+10, LV_ANIM_OFF);
    lv_bar_set_start_value(y_bar, att_y-10, LV_ANIM_OFF);
    lv_bar_set_value(y_bar, att_y+10, LV_ANIM_OFF);
    lv_bar_set_start_value(z_bar, att_z-10, LV_ANIM_OFF);
    lv_bar_set_value(z_bar, att_z+10, LV_ANIM_OFF);

    // 判断运动状态
    uint8_t status = qmi8658_fetch_motion();
    if (status & 0x20) // 判断是否发生Any-Motion
    {
        lv_label_set_text(att_label, "运动或震动");
    }
    else if (status & 0x40) // 判断是否发生No-Motion
    {
        lv_label_set_text(att_label, "静止");
    }
    else if (status & 0x80) // 判断是否发生Significant-Motion
    {
        lv_label_set_text(att_label, "剧烈运动");
    }
}

// 姿态监测处理任务
static void task_process_att(void *arg)
{
    esp_err_t ret = qmi8658_init();
    if (ret != ESP_OK){ // 如果传感器初始化不成功
        // 液晶屏提醒用户 传感器错误
        lvgl_port_lock(0);
        lv_obj_t * label = lv_label_create(icon_in_obj);
        lv_label_set_text(label, "QMI8658传感器错误...");
        lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0); 
        lv_obj_set_style_text_font(label, &font_alipuhui20, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        lvgl_port_unlock();
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 提示词保留1秒
        lvgl_port_lock(0);
        lv_obj_del(icon_in_obj); // 删除画布 回到主界面
        lvgl_port_unlock();
    }
    else{ // 传感器初始化成功
        lvgl_port_lock(0);
        // 显示x角度值
        label_x = lv_label_create(icon_in_obj);
        lv_label_set_text(label_x, "X:");
        lv_obj_set_style_text_color(label_x, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_font(label_x, &lv_font_montserrat_20, 0);
        lv_obj_align(label_x, LV_ALIGN_TOP_LEFT, 20, 60);
        // 显示x角度bar
        x_bar = lv_bar_create(icon_in_obj);
        lv_obj_set_size(x_bar, 200, 25);
        lv_obj_align(x_bar, LV_ALIGN_TOP_LEFT, 80, 60);
        lv_bar_set_mode(x_bar, LV_BAR_MODE_RANGE);
        lv_bar_set_range(x_bar, -101, 101);
        lv_bar_set_start_value(x_bar, -10, LV_ANIM_OFF);
        lv_bar_set_value(x_bar, 10, LV_ANIM_OFF);

        // 显示y角度值
        label_y = lv_label_create(icon_in_obj);
        lv_label_set_text(label_y, "Y:");
        lv_obj_set_style_text_color(label_y, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_font(label_y, &lv_font_montserrat_20, 0);
        lv_obj_align(label_y, LV_ALIGN_TOP_LEFT, 20, 120);
        // 显示y角度bar
        y_bar = lv_bar_create(icon_in_obj);
        lv_obj_set_size(y_bar, 200, 25);
        lv_obj_align(y_bar, LV_ALIGN_TOP_LEFT, 80, 120);
        lv_bar_set_mode(y_bar, LV_BAR_MODE_RANGE);
        lv_bar_set_range(y_bar, -101, 101);
        lv_bar_set_start_value(y_bar, -10, LV_ANIM_OFF);
        lv_bar_set_value(y_bar, 10, LV_ANIM_OFF);

        // 显示z角度值
        label_z = lv_label_create(icon_in_obj);
        lv_label_set_text(label_z, "Z:");
        lv_obj_set_style_text_color(label_z, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_font(label_z, &lv_font_montserrat_20, 0);
        lv_obj_align(label_z, LV_ALIGN_TOP_LEFT, 20, 180);
        // 显示z角度bar
        z_bar = lv_bar_create(icon_in_obj);
        lv_obj_set_size(z_bar, 200, 25);
        lv_obj_align(z_bar, LV_ALIGN_TOP_LEFT, 80, 180);
        lv_bar_set_mode(z_bar, LV_BAR_MODE_RANGE);
        lv_bar_set_range(z_bar, -101, 101);
        lv_bar_set_start_value(z_bar, -10, LV_ANIM_OFF);
        lv_bar_set_value(z_bar, 10, LV_ANIM_OFF);

        lvgl_port_unlock();

        // 创建一个lv_timer 用于更新角度
        my_lv_timer = lv_timer_create(att_update_cb, 200, NULL); 
    }

    vTaskDelete(NULL);
}

void att_event_handler(lv_event_t * e)
{
    // 创建一个界面
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
    lv_obj_t *att_title = lv_obj_create(icon_in_obj);
    lv_obj_set_size(att_title, 320, 40);
    lv_obj_set_style_pad_all(att_title, 0, 0);  // 设置间隙
    lv_obj_align(att_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(att_title, lv_color_hex(0x30a830), 0);
    // 显示标题
    att_label = lv_label_create(att_title);
    lv_label_set_text(att_label, "运动监测");
    lv_obj_set_style_text_color(att_label, lv_color_hex(0xffffff), 0); 
    lv_obj_set_style_text_font(att_label, &font_alipuhui20, 0);
    lv_obj_align(att_label, LV_ALIGN_CENTER, 0, 0);
    // 创建后退按钮
    btn_att_back = lv_btn_create(att_title);
    lv_obj_align(btn_att_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_size(btn_att_back, 60, 30);
    lv_obj_set_style_border_width(btn_att_back, 0, 0); // 设置边框宽度
    lv_obj_set_style_pad_all(btn_att_back, 0, 0);  // 设置间隙
    lv_obj_set_style_bg_opa(btn_att_back, LV_OPA_TRANSP, LV_PART_MAIN); // 背景透明
    lv_obj_set_style_shadow_opa(btn_att_back, LV_OPA_TRANSP, LV_PART_MAIN); // 阴影透明
    lv_obj_add_event_cb(btn_att_back, btn_att_back_cb, LV_EVENT_CLICKED, NULL); // 添加按键处理函数

    lv_obj_t *label_back = lv_label_create(btn_att_back); 
    lv_label_set_text(label_back, LV_SYMBOL_LEFT);  // 按键上显示左箭头符号
    lv_obj_set_style_text_font(label_back, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_back, lv_color_hex(0xffffff), 0); 
    lv_obj_align(label_back, LV_ALIGN_CENTER, -10, 0);

    icon_flag = 1; // 标记已经进入第一个应用
    xTaskCreatePinnedToCore(task_process_att, "task_process_att", 2 * 1024, NULL, 5, NULL, 1);
}


/******************************** 第3个图标 SD卡 应用程序***************************************************************************/
lv_obj_t * sdcard_title; // SD卡页面标题背景
lv_obj_t * sdcard_label; // SD卡页面标题
lv_obj_t * sdcard_file_list; // SD卡文件列表

extern sdmmc_card_t *sdmmc_card;

struct file_path_info
{
    uint8_t path_index;  // 在第几级目录
    char path_now[512]; // 当前文件路径
    char path_back[512]; // 上级文件路径
};
struct file_path_info file_path_info;

// 函数声明
esp_err_t list_sdcard_files(char * path);
static void file_list_btn_cb(lv_event_t * e); 

// 返回主界面按钮事件处理函数
static void btn_sdback_cb(lv_event_t * e)
{
    if (file_path_info.path_index == 0){ // 如果当前是根目录
        bsp_sdcard_unmount(); // 卸载SD卡
        lv_obj_del(icon_in_obj); // 回到主界面
    }else{
        lv_obj_clean(sdcard_file_list); // 清除当前wifi列表
        esp_err_t ret = list_sdcard_files(file_path_info.path_back); // 列出上一级目录文件
        if (ret == ESP_OK){ // 如果成功列出目录
            strcpy(file_path_info.path_now, file_path_info.path_back); // 刚刚进入的这个目录路径 变成当前路径
            file_path_info.path_index--; // 目录级数索引退一级
            // 计算再向下退一级的目录路径
            char *slash = strrchr(file_path_info.path_back, '/'); // 从后往前查找字符'/'
            if (slash!= NULL) { // 如果查找到
                *slash = '\0'; // 替换为NULL 表示字符串结束
            }
            ESP_LOGI(TAG, "path_index: %d", file_path_info.path_index);
            ESP_LOGI(TAG, "path_now: %s", file_path_info.path_now);
            ESP_LOGI(TAG, "path_back: %s", file_path_info.path_back);
        }
    }
}

// 列出SD卡中的文件
esp_err_t list_sdcard_files(char * path) 
{
    esp_err_t ret;
    DIR *dir;
    struct dirent *ent;
    lv_obj_t * btn;
    if ((dir = opendir(path))!= NULL) { // 打开目录
        while ((ent = readdir(dir))!= NULL) { // 读取目录中的文件
            /* 常规文件处理 */
            if (ent->d_type == DT_REG){ // 如果是常规文件
                int file_type_flag = 0;
                const char* extension = strrchr(ent->d_name, '.'); // 从后往前 找到字符'.'
                if (extension != NULL) { // 如果找到了'.'
                    extension++; // 指针地址+1
                    if (strcmp(extension, "mp3") == 0) { // 如果是mp3
                        file_type_flag = 1; // 标记为音乐文件
                    } else if (strcmp(extension, "wav") == 0) { // 如果是wav
                        file_type_flag = 1; // 标记为音乐文件
                    } else if (strcmp(extension, "mp4") == 0) {
                        file_type_flag = 2; // 标记为视频文件
                    } else if (strcmp(extension, "avi") == 0) {
                        file_type_flag = 2; // 标记为视频文件
                    } else if (strcmp(extension, "jpg") == 0) {
                        file_type_flag = 3; // 标记为图片
                    } else if (strcmp(extension, "jpeg") == 0) {
                        file_type_flag = 3; // 标记为图片
                    } else if (strcmp(extension, "png") == 0) {
                        file_type_flag = 3; // 标记为图片
                    } else if (strcmp(extension, "bmp") == 0) {
                        file_type_flag = 3; // 标记为图片
                    } else if (strcmp(extension, "gif") == 0) {
                        file_type_flag = 3; // 标记为图片
                    } else {
                        file_type_flag = 0; // 除了以上文件 其它文件都归类为普通文件
                    }
                }
                lvgl_port_lock(0);
                switch (file_type_flag)
                {
                case 1:
                    btn = lv_list_add_btn(sdcard_file_list, LV_SYMBOL_AUDIO, (const char *)ent->d_name);  // 显示音乐文件图标
                    break;
                case 2:
                    btn = lv_list_add_btn(sdcard_file_list, LV_SYMBOL_VIDEO, (const char *)ent->d_name);  // 显示视频文件图标
                    break;
                case 3:
                    btn = lv_list_add_btn(sdcard_file_list, LV_SYMBOL_IMAGE, (const char *)ent->d_name);  // 显示图片文件图标
                    break;
                default:
                    btn = lv_list_add_btn(sdcard_file_list, LV_SYMBOL_FILE, (const char *)ent->d_name);  // 显示普通文件图标
                    break;
                }
                lv_obj_t *icon = lv_obj_get_child(btn, 0); // 获取图标指针
                lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0); // 修改图标的字体    
                lv_obj_add_event_cb(btn, file_list_btn_cb, LV_EVENT_CLICKED, NULL);  // 添加点击回调函数
                lvgl_port_unlock();
            }
            /* 文件夹处理 */
            else if (ent->d_type == DT_DIR) { // 如果是文件夹
                lvgl_port_lock(0);
                btn = lv_list_add_btn(sdcard_file_list, LV_SYMBOL_DIRECTORY, (const char *)ent->d_name); 
                lv_obj_t *icon = lv_obj_get_child(btn, 0); // 获取图标指针
                lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0); // 修改图标的字体
                lv_obj_add_event_cb(btn, file_list_btn_cb, LV_EVENT_CLICKED, NULL); // 添加点击回调函数
                lvgl_port_unlock();
            }
        }
        closedir(dir);
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to open directory %s.", path);
        ret = ESP_FAIL;
    }
    return ret;
}

// 文件点击 事件处理函数
static void file_list_btn_cb(lv_event_t * e)
{
    const char *file_name = NULL; // 当前文件名称
    // 获取点击的按钮名称 即文件名称
    file_name = lv_list_get_btn_text(lv_obj_get_parent(e->target), e->target);
    ESP_LOGI(TAG, "file name: %s", file_name);
    // 列出 SD 卡中的文件
    struct stat st; // 获取文件状态信息结构体
    strcpy(file_path_info.path_back, file_path_info.path_now); // 保存上一级目录
    strcat(file_path_info.path_now, "/");
    strcat(file_path_info.path_now, file_name);
    if (stat(file_path_info.path_now, &st) == 0){ // 如果成功获取到状态信息
        if (S_ISDIR(st.st_mode)){ // 如果是目录
            lv_obj_clean(sdcard_file_list); // 清除当前wifi列表
            esp_err_t ret = list_sdcard_files(file_path_info.path_now);
            if (ret == ESP_OK){ // 如果成功列出了目录
                file_path_info.path_index++; // 表示进入到下一集目录
                ESP_LOGI(TAG, "path_index: %d", file_path_info.path_index);
                ESP_LOGI(TAG, "path_now: %s", file_path_info.path_now);
                ESP_LOGI(TAG, "path_back: %s", file_path_info.path_back);
            }
            return;
        }
    }
    // 如果没有成功进入目录
    strcpy(file_path_info.path_now, file_path_info.path_back); // 没有列出新的列表 还原当前路径
    // 还原再向下退一级的目录路径
    char *slash = strrchr(file_path_info.path_back, '/'); // 从后往前查找字符'/'
    if (slash!= NULL) { // 如果查找到
        *slash = '\0'; // 替换为NULL 表示字符串结束
    }
    ESP_LOGI(TAG, "path_index: %d", file_path_info.path_index);
    ESP_LOGI(TAG, "path_now: %s", file_path_info.path_now);
    ESP_LOGI(TAG, "path_back: %s", file_path_info.path_back);
}

// SD卡处理任务
static void task_process_sdcard(void *arg)
{
    esp_err_t ret;

    ret = bsp_sdcard_mount(); // 挂载SD卡
    if(ret != ESP_OK){ // 如果没有挂载成功
        ESP_LOGE(TAG, "Failed to mount filesystem.");
        lvgl_port_lock(0);
        lv_label_set_text(sdcard_label, "SD卡挂载不成功");
        lvgl_port_unlock();
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 给上面一点显示的时间
        lvgl_port_lock(0);
        lv_obj_del(icon_in_obj);
        lvgl_port_unlock();
    }else{ // 如果挂载成功
        // 终端显示SD卡信息
        sdmmc_card_print_info(stdout, sdmmc_card);
        // 液晶屏标题栏显示SD卡容量
        lvgl_port_lock(0);
        lv_label_set_text_fmt(sdcard_label, "SD: %lluGB",
            (((uint64_t)sdmmc_card->csd.capacity) * sdmmc_card->csd.sector_size) >> 30);
        lvgl_port_unlock();

        // 创建返回按钮
        lvgl_port_lock(0);
        lv_obj_t *btn_back = lv_btn_create(sdcard_title);
        lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_size(btn_back, 60, 30);
        lv_obj_set_style_border_width(btn_back, 0, 0); // 设置边框宽度
        lv_obj_set_style_pad_all(btn_back, 0, 0);  // 设置间隙
        lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN); // 背景透明
        lv_obj_set_style_shadow_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN); // 阴影透明
        lv_obj_add_event_cb(btn_back, btn_sdback_cb, LV_EVENT_CLICKED, NULL); // 添加按键处理函数

        lv_obj_t *label_back = lv_label_create(btn_back); 
        lv_label_set_text(label_back, LV_SYMBOL_LEFT);  // 按键上显示左箭头符号
        lv_obj_set_style_text_font(label_back, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(label_back, lv_color_hex(0xffffff), 0); 
        lv_obj_align(label_back, LV_ALIGN_CENTER, -10, 0);

        // 创建文件列表
        sdcard_file_list = lv_list_create(icon_in_obj);
        lv_obj_set_size(sdcard_file_list, 320, 200);
        lv_obj_align(sdcard_file_list, LV_ALIGN_TOP_LEFT, 0, 40);
        lv_obj_set_style_border_width(sdcard_file_list, 0, 0);
        lv_obj_set_style_text_font(sdcard_file_list, &font_alipuhui20, 0);
        lv_obj_set_scrollbar_mode(sdcard_file_list, LV_SCROLLBAR_MODE_OFF); // 隐藏wifi_list滚动条
        lvgl_port_unlock();
        // 列出 SD 卡中的文件
        file_path_info.path_index = 0; // 表示当前在根目录
        strcpy(file_path_info.path_now, SD_MOUNT_POINT); // 装入当前路径
        ret = list_sdcard_files(file_path_info.path_now);
        if(ret != ESP_OK)
        {
            lvgl_port_lock(0);
            lv_label_set_text(sdcard_label, "SD卡目录打开失败");
            lvgl_port_unlock();
            bsp_sdcard_unmount();
        }
    }
    
    vTaskDelete(NULL);
}


// 进入SD卡应用程序
void sdcard_event_handler(lv_event_t * e)
{
    // 创建一个界面对象
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
    sdcard_title = lv_obj_create(icon_in_obj);
    lv_obj_set_size(sdcard_title, 320, 40);
    lv_obj_set_style_pad_all(sdcard_title, 0, 0);  // 设置间隙
    lv_obj_align(sdcard_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(sdcard_title, lv_color_hex(0x008b8b), 0);
    // 显示标题
    sdcard_label = lv_label_create(sdcard_title);
    lv_label_set_text(sdcard_label, "TF卡扫描中...");
    lv_obj_set_style_text_color(sdcard_label, lv_color_hex(0xffffff), 0); 
    lv_obj_set_style_text_font(sdcard_label, &font_alipuhui20, 0);
    lv_obj_align(sdcard_label, LV_ALIGN_CENTER, 0, 0);

    icon_flag = 3; // 标记已经进入第三个应用

    xTaskCreatePinnedToCore(task_process_sdcard, "task_process_sdcard", 3 * 1024, NULL, 5, NULL, 1);
}



/******************************** 第4个图标 摄像头 应用程序 *****************************************************************************/
lv_obj_t * img_camera;

// 摄像头图像
lv_img_dsc_t img_camera_dsc = {
  .header.cf = LV_IMG_CF_TRUE_COLOR,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 320,
  .header.h = 240,
  .data_size = 240*320*2,
};

// 摄像头处理任务
static void task_process_camera(void *arg)
{   
    while (icon_flag == 4)
    {
        camera_fb_t *frame = esp_camera_fb_get();
        img_camera_dsc.data = frame->buf;
        lv_img_set_src(img_camera, &img_camera_dsc);
        esp_camera_fb_return(frame);
    }
    esp_camera_deinit(); // 取消初始化摄像头
    lvgl_port_lock(0);
    lv_obj_del(icon_in_obj); // 删除摄像头画布
    lvgl_port_unlock();
    dvp_pwdn(1); // 摄像头进入掉电模式
    vTaskDelete(NULL);
}

// 返回主界面按钮事件处理函数
static void btn_camback_cb(lv_event_t * e)
{
    icon_flag = 0;
}

// 进入摄像头应用
void camera_event_handler(lv_event_t * e)
{
    bsp_camera_init(); // 摄像头初始化
    // 创建一个界面对象
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_radius(&style, 10);  
    lv_style_set_bg_opa( &style, LV_OPA_COVER );
    lv_style_set_bg_color(&style, lv_color_hex(0xcccccc));
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    lv_style_set_width(&style, 320);  
    lv_style_set_height(&style, 240); 

    icon_in_obj = lv_obj_create(lv_scr_act());
    lv_obj_add_style(icon_in_obj, &style, 0);

    img_camera = lv_img_create(icon_in_obj);
    lv_obj_set_pos(img_camera, 0, 0);
    lv_obj_set_size(img_camera, 320, 240);

    // 创建返回按钮
    lv_obj_t *btn_back = lv_btn_create(icon_in_obj);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_set_style_border_width(btn_back, 0, 0); // 设置边框宽度
    lv_obj_set_style_pad_all(btn_back, 0, 0);  // 设置间隙
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN); // 背景透明
    lv_obj_set_style_shadow_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN); // 阴影透明
    lv_obj_add_event_cb(btn_back, btn_camback_cb, LV_EVENT_CLICKED, NULL); // 添加按键处理函数

    lv_obj_t *label_back = lv_label_create(btn_back); 
    lv_label_set_text(label_back, LV_SYMBOL_LEFT);  // 按键上显示左箭头符号
    lv_obj_set_style_text_font(label_back, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_back, lv_color_hex(0xffffff), 0); 
    lv_obj_align(label_back, LV_ALIGN_CENTER, -10, 0);

    icon_flag = 4; // 标记已经进入第四个应用

    xTaskCreatePinnedToCore(task_process_camera, "task_process_camera", 4 * 1024, NULL, 5, NULL, 1);
}


/******************************** 第5个图标 WiFi设置 应用程序*****************************************************************************/
/******************************** 第6个图标 蓝牙设置 应用程序***********************************************************************************/
lv_obj_t * ble_label;
lv_obj_t * btn_ble_back;

// 返回主界面按钮事件处理函数
static void btn_ble_back_cb(lv_event_t * e)
{
    bt_hid_end();
    lv_obj_del(icon_in_obj);
    icon_flag = 0;
}

// 进入蓝牙设置应用
void btset_event_handler(lv_event_t * e)
{
    // 创建一个界面对象
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
    lv_obj_t *ble_title = lv_obj_create(icon_in_obj);
    lv_obj_set_size(ble_title, 320, 40);
    lv_obj_set_style_pad_all(ble_title, 0, 0);  // 设置间隙
    lv_obj_align(ble_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(ble_title, lv_color_hex(0xb87fa8), 0);
    // 显示标题
    ble_label = lv_label_create(ble_title);
    lv_label_set_text(ble_label, "蓝牙控制器");
    lv_obj_set_style_text_color(ble_label, lv_color_hex(0xffffff), 0); 
    lv_obj_set_style_text_font(ble_label, &font_alipuhui20, 0);
    lv_obj_align(ble_label, LV_ALIGN_CENTER, 0, 0);
    // 创建后退按钮
    btn_ble_back = lv_btn_create(ble_title);
    lv_obj_align(btn_ble_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_size(btn_ble_back, 60, 30);
    lv_obj_set_style_border_width(btn_ble_back, 0, 0); // 设置边框宽度
    lv_obj_set_style_pad_all(btn_ble_back, 0, 0);  // 设置间隙
    lv_obj_set_style_bg_opa(btn_ble_back, LV_OPA_TRANSP, LV_PART_MAIN); // 背景透明
    lv_obj_set_style_shadow_opa(btn_ble_back, LV_OPA_TRANSP, LV_PART_MAIN); // 阴影透明
    lv_obj_add_event_cb(btn_ble_back, btn_ble_back_cb, LV_EVENT_CLICKED, NULL); // 添加按键处理函数

    lv_obj_t *label_back = lv_label_create(btn_ble_back); 
    lv_label_set_text(label_back, LV_SYMBOL_LEFT);  // 按键上显示左箭头符号
    lv_obj_set_style_text_font(label_back, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_back, lv_color_hex(0xffffff), 0); 
    lv_obj_align(label_back, LV_ALIGN_CENTER, -10, 0);

    app_hid_ctrl();

    icon_flag = 6; // 标记已经进入第6个应用
}



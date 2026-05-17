#include "app_ui_internal.h"

LV_FONT_DECLARE(font_alipuhui20);

lv_obj_t * main_obj; // 主界面
lv_obj_t * main_text_label; // 主界面 欢迎语
lv_obj_t * icon_in_obj; // 应用界面
int icon_flag; // 标记现在进入哪个应用 在主界面时为0

/*********************** 开机界面 ****************************/
lv_obj_t * lckfb_logo;
// 设置角度的回调函数
static void set_angle(void * img, int32_t v)
{
    lv_img_set_angle(img, v); // 设置图片的旋转角度
}
// 开机界面
void lv_gui_start(void)
{
    lvgl_port_lock(0);
    // 显示logo
    LV_IMG_DECLARE(image_lckfb_logo);  // 声明图片
    lckfb_logo = lv_img_create(lv_scr_act()); // 创建图片对象
    lv_img_set_src(lckfb_logo, &image_lckfb_logo); // 设置图片对象的图片源
    lv_obj_align(lckfb_logo, LV_ALIGN_CENTER, 0, 0); // 设置图片位置为屏幕正中心
    lv_img_set_pivot(lckfb_logo, 60, 60); // 设置图片围绕自己的中心位置旋转   

    // 设置旋转动画
    lv_anim_t a; // 创建动画变量
    lv_anim_init(&a); // 初始化动画变量
    lv_anim_set_var(&a, lckfb_logo); // 动画变量赋值为logo图片
    lv_anim_set_exec_cb(&a, set_angle); // 创建设置角度的回调函数
    lv_anim_set_values(&a, 0, 3600); // 设置动画旋转角度的开始值和结尾值
    lv_anim_set_time(&a, 200); // 设置转一圈的周期是200毫秒
    lv_anim_set_repeat_count(&a, 5); // 设置旋转5次
    lv_anim_start(&a); // 动画开始
    lvgl_port_unlock();
}



/******************************** 主界面  ******************************/

void lv_main_page(void)
{
    lvgl_port_lock(0);

    lv_obj_del(lckfb_logo); // 删除开机logo
    // 创建主界面基本对象
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0); // 修改背景为黑色

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_radius(&style, 10);  
    lv_style_set_bg_opa( &style, LV_OPA_COVER );
    lv_style_set_bg_color(&style, lv_color_hex(0x00BFFF));
    lv_style_set_bg_grad_color( &style, lv_color_hex( 0x00BF00 ) );
    lv_style_set_bg_grad_dir( &style, LV_GRAD_DIR_VER );
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    lv_style_set_width(&style, 320);  
    lv_style_set_height(&style, 240); 

    main_obj = lv_obj_create(lv_scr_act());
    lv_obj_add_style(main_obj, &style, 0);

    // 显示右上角符号
    lv_obj_t * sylbom_label = lv_label_create(main_obj);
    lv_obj_set_style_text_font(sylbom_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sylbom_label, lv_color_hex(0xffffff), 0);
    lv_label_set_text(sylbom_label, LV_SYMBOL_BLUETOOTH" "LV_SYMBOL_WIFI);  // 显示蓝牙和wifi图标
    lv_obj_align_to(sylbom_label, main_obj, LV_ALIGN_TOP_RIGHT, -10, 10);

    // 显示左上角欢迎语
    main_text_label = lv_label_create(main_obj);
    lv_obj_set_style_text_font(main_text_label, &font_alipuhui20, 0);
    lv_label_set_long_mode(main_text_label, LV_LABEL_LONG_SCROLL_CIRCULAR);     /*Circular scroll*/
    lv_obj_set_width(main_text_label, 280);
    lv_label_set_text(main_text_label, "欢迎使用立创实战派开发板");
    lv_obj_align_to(main_text_label, main_obj, LV_ALIGN_TOP_LEFT, 8, 5);

    // 设置应用图标style
    static lv_style_t btn_style;
    lv_style_init(&btn_style);
    lv_style_set_radius(&btn_style, 16);  
    lv_style_set_bg_opa( &btn_style, LV_OPA_COVER );
    lv_style_set_text_color(&btn_style, lv_color_hex(0xffffff)); 
    lv_style_set_border_width(&btn_style, 0);
    lv_style_set_pad_all(&btn_style, 5);
    lv_style_set_width(&btn_style, 80);  
    lv_style_set_height(&btn_style, 80); 

    // 创建第1个应用图标
    lv_obj_t *icon1 = lv_btn_create(main_obj);
    lv_obj_add_style(icon1, &btn_style, 0);
    lv_obj_set_style_bg_color(icon1, lv_color_hex(0x30a830), 0);
    lv_obj_set_pos(icon1, 15, 50);
    lv_obj_add_event_cb(icon1, att_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t * img1 = lv_img_create(icon1);
    LV_IMG_DECLARE(img_att_icon);
    lv_img_set_src(img1, &img_att_icon);
    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

    // 创建第2个应用图标
    lv_obj_t *icon2 = lv_btn_create(main_obj);
    lv_obj_add_style(icon2, &btn_style, 0);
    lv_obj_set_style_bg_color(icon2, lv_color_hex(0xf87c30), 0);
    lv_obj_set_pos(icon2, 120, 50);
    lv_obj_add_event_cb(icon2, music_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t * img2 = lv_img_create(icon2);
    LV_IMG_DECLARE(img_music_icon);
    lv_img_set_src(img2, &img_music_icon);
    lv_obj_align(img2, LV_ALIGN_CENTER, 0, 0);

    // 创建第3个应用图标
    lv_obj_t *icon3 = lv_btn_create(main_obj);
    lv_obj_add_style(icon3, &btn_style, 0);
    lv_obj_set_style_bg_color(icon3, lv_color_hex(0x008b8b), 0);
    lv_obj_set_pos(icon3, 225, 50);
    lv_obj_add_event_cb(icon3, sdcard_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t * img3 = lv_img_create(icon3);
    LV_IMG_DECLARE(img_sd_icon);
    lv_img_set_src(img3, &img_sd_icon);
    lv_obj_align(img3, LV_ALIGN_CENTER, 0, 0);

    // 创建第4个应用图标
    lv_obj_t *icon4 = lv_btn_create(main_obj);
    lv_obj_add_style(icon4, &btn_style, 0);
    lv_obj_set_style_bg_color(icon4, lv_color_hex(0xd8b010), 0);
    lv_obj_set_pos(icon4, 15, 147);
    lv_obj_add_event_cb(icon4, camera_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t * img4 = lv_img_create(icon4);
    LV_IMG_DECLARE(img_camera_icon);
    lv_img_set_src(img4, &img_camera_icon);
    lv_obj_align(img4, LV_ALIGN_CENTER, 0, 0);

    // 创建第5个应用图标
    lv_obj_t *icon5 = lv_btn_create(main_obj);
    lv_obj_add_style(icon5, &btn_style, 0);
    lv_obj_set_style_bg_color(icon5, lv_color_hex(0xcd5c5c), 0);
    lv_obj_set_pos(icon5, 120, 147);
    lv_obj_add_event_cb(icon5, wifiset_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t * img5 = lv_img_create(icon5);
    LV_IMG_DECLARE(img_wifiset_icon);
    lv_img_set_src(img5, &img_wifiset_icon);
    lv_obj_align(img5, LV_ALIGN_CENTER, 0, 0);

    // 创建第6个应用图标
    lv_obj_t *icon6 = lv_btn_create(main_obj);
    lv_obj_add_style(icon6, &btn_style, 0);
    lv_obj_set_style_bg_color(icon6, lv_color_hex(0xb87fa8), 0);
    lv_obj_set_pos(icon6, 225, 147);
    lv_obj_add_event_cb(icon6, btset_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t * img6 = lv_img_create(icon6);
    LV_IMG_DECLARE(img_btset_icon);
    lv_img_set_src(img6, &img_btset_icon);
    lv_obj_align(img6, LV_ALIGN_CENTER, 0, 0);

    lvgl_port_unlock();
}


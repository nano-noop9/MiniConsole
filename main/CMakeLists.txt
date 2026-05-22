idf_component_register(SRCS "weather_api.c" "esp32_s3_szp.c" "main.c" "app_ui.c" "app_ui_apps.c" "app_ui_music.c" "app_ui_wifi.c" "app_ui_base.c"
                       SRCS "assets/image_lckfb_logo.c"
                       SRCS "assets/hello_img.c"
                       SRCS "assets/img_att_icon.c"
                       SRCS "assets/img_btset_icon.c"
                       SRCS "assets/img_camera_icon.c"
                       SRCS "assets/img_music_icon.c"
                       SRCS "assets/img_sd_icon.c"
                       SRCS "assets/img_wifiset_icon.c"
                       SRCS "assets/app_img.c"
                       SRCS "assets/weather_img.c"
                       SRCS "assets/Clock_img.c"
                       SRCS "assets/font_alipuhui20.c"
                       SRCS "bt/ble_hidd_demo.c" "bt/esp_hidd_prf_api.c" "bt/hid_dev.c" "bt/hid_device_le_prf.c"
                       EMBED_FILES "sword.pcm"
                       INCLUDE_DIRS "."
                       INCLUDE_DIRS "bt")

# spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)

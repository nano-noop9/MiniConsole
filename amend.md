# 工程改动说明

## 6. 音乐播放器后台播放与 IO0 播放/暂停键

- 音乐播放器退出逻辑改为只关闭 LVGL 界面，不再调用 `audio_player_stop()`、`audio_player_delete()` 和 `bsp_sdcard_unmount()`，因此返回主界面后音乐可以继续播放。
- 音乐播放器相关 UI 指针初始化为 `NULL`，退出界面后会清空 `music_list`、`label_play_pause`、`btn_play_pause`、`volume_slider`、`music_title_label`、`btn_music_back`，避免后台播放回调访问已删除的 LVGL 对象。
- 音频播放回调在更新下拉列表和播放/暂停按钮前增加空指针判断；界面已退出时只继续播放和自动切歌，不再操作 UI。
- 再次进入音乐播放器时，如果播放器已经初始化，则不重复创建播放器、不重新挂载 SD 卡，只重新创建界面并同步当前歌曲和播放/暂停按钮状态。
- 新增 `music_play_pause_toggle()` 作为统一播放/暂停切换接口：播放中暂停，暂停中继续播放，空闲时从当前 `music_file_index` 开始播放。
- 新增 IO0 用户按键初始化：`BSP_USER_KEY_GPIO` 定义为 `GPIO_NUM_0`，`bsp_user_key_init()` 将 IO0 配置为输入、内部上拉、低电平按下。
- 新增 `user_key_task` 轮询 IO0 并做简单消抖，检测到一次有效按下后调用 `music_play_pause_toggle()`，松开后允许下一次触发。
- `app_main()` 初始化阶段调用 `bsp_user_key_init()`，使 BOOT/用户键可用于切换音乐播放和暂停。

## 7. SD 卡挂载共享修复

- 修复音乐后台播放后再进入 SD 卡 app 时显示“SD 卡挂载不成功”的问题。
- 原因是音乐播放器后台播放期间会保持 SD 卡挂载，SD 卡 app 再次调用 `bsp_sdcard_mount()` 会触发重复挂载失败。
- 在 `bsp_sdcard_mount()` / `bsp_sdcard_unmount()` 中加入挂载引用计数：
  - SD 卡已经挂载时，重复调用 `bsp_sdcard_mount()` 直接返回 `ESP_OK`，并增加引用计数。
  - 调用 `bsp_sdcard_unmount()` 时先减少引用计数。
  - 只有引用计数归零时才真正执行 `esp_vfs_fat_sdcard_unmount()`。
- 这样音乐播放器和 SD 卡 app 可以共享同一次 SD 卡挂载，SD 卡页面退出不会中断后台音乐播放。

## 8. BLE HID 广播、连接能力与退出流程修复

- 修复 BLE HID 启动后广播失败的问题。故障日志表现为 `Feature Config ... CONNECT:0`，随后出现 `bta_dm_ble_set_adv_params_all(), fail to set ble adv params.`、`hci write adv params error 0x12` 和 `advertising start failed, status = 13`。
- 根因是 HID 当前使用 `ADV_TYPE_IND` 可连接广播，但蓝牙 controller 编译配置中 BLE connection feature 未启用。ESP-IDF 5.2.6 下需要开启 `CONFIG_BT_CTRL_BLE_MASTER=y`，该选项虽然名字带 `MASTER`，实际说明为启用 BLE connection feature；未开启时不建议使用 connectable ADV。
- 蓝牙内存配置增加：
  - `CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST=y`：让蓝牙优先从 PSRAM 分配可外置的内存，降低内部 DRAM 压力。
  - `CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY=y`：启用 BLE 动态环境内存，减少固定占用。
  - `CONFIG_BT_CTRL_BLE_MASTER=y`：启用 BLE connection feature，使 HID 可连接广播能够正常设置并启动。
- 初始异常时内部 DRAM 非常紧张，曾出现 `Free: 6351 bytes`、`DRAM_Largest_block: 3200 bytes`、`DRAM Used: 97.49%`。配置优化后，蓝牙关闭时 DRAM 可恢复到约 `145KB free`；蓝牙运行时仍会占用较多内部 DRAM，需要继续关注 `DRAM Free` 和 `DRAM_Largest_block`。
- `main/bt/ble_hidd_demo.c` 增加 BLE HID 启停状态保护：
  - `bt_hid_started`：避免重复进入蓝牙页面时重复初始化 controller、Bluedroid 和 HID profile。
  - `bt_adv_started`：记录广播是否已经启动，用于退出时判断是否需要停止广播。
  - `bt_hid_stopping`：退出蓝牙页面时标记正在关闭，阻止断连回调再次启动广播。
  - `classic_bt_mem_released`：避免重复调用 `esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)`。
- 新增 `hidd_start_advertising()` 统一封装 `esp_ble_gap_start_advertising()`，并检查返回值。这样广播失败时会打印明确的 ESP 错误名，而不是只依赖底层 HCI 日志。
- BLE GAP 回调增加广播状态日志：
  - `ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT`：检查广播数据配置是否成功，成功后再启动广播。
  - `ESP_GAP_BLE_ADV_START_COMPLETE_EVT`：记录广播启动成功或失败，并同步 `bt_adv_started`。
  - `ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT`：记录广播停止结果，并清除 `bt_adv_started`。
- 启动阶段增加返回值检查：
  - 检查 `esp_ble_gap_set_device_name()` 和 `esp_ble_gap_config_adv_data()`。
  - 检查 `esp_ble_gap_register_callback()` 和 `esp_hidd_register_callbacks()`。
  - 检查 controller、Bluedroid、HID profile 初始化/启用结果。
- 初始化失败路径补充清理逻辑，避免 controller、Bluedroid 或 HID profile 半初始化后残留状态，影响下一次进入蓝牙页面。
- 退出蓝牙页面时，`bt_hid_end()` 会先设置 `bt_hid_stopping = true`，再停止广播并 deinit HID/Bluedroid/controller。这样连接断开回调收到 `ESP_HIDD_EVENT_BLE_DISCONNECT` 时不会再次调用 `hidd_start_advertising()`。
- 验证结果：
  - BLE HID 广播和连接已恢复正常。
  - 退出蓝牙页面时不再出现 `advertising start failed, status = 13`。
  - 退出时剩余 `disconnect`、`disc complete`、`BTA_DISABLE_DELAY set to 200 ms` 属于蓝牙栈主动断开连接和延迟 disable 的正常日志。
- 当前蓝牙生命周期仍然是“进入蓝牙页面启动，退出蓝牙页面关闭”，不是蓝牙常开。如果后续要改成蓝牙常开，应改为开机或进入主界面后启动一次 `bt_hid_start()`，退出蓝牙页面只删除 UI，不再调用 `bt_hid_end()`。
- PSRAM 很空并不代表内部 DRAM 可以完全释放。DMA buffer、WiFi/BLE controller、任务栈、驱动内部结构等仍主要依赖内部 DRAM；PSRAM 更适合放大图片、音频缓存、摄像头帧缓冲、较大的业务缓存等。
- 本次未主动执行 `idf.py build`、`idf.py flash` 或烧录；用户通过 `idf.py -p COM11 app-flash monitor` 验证 BLE HID 行为和日志。

## 9. Clock 页面与 WiFi 时间显示修复

- 新增 `main/app_ui_base.c` 作为主页 Clock 应用页面，实现独立的时钟页面 UI。
- 修复点击主页 Clock 应用后重启的问题：
  - 根因是 `clock_time_label` 声明后没有创建，就被传入 LVGL 样式设置函数。
  - Clock 页面改用独立的 `clock_date_label` / `clock_time_label`，避免覆盖 WiFi 时间标签。
- app 页面顶层时间标签改名并拆分职责：
  - 原 `date_label` / `time_label` 改为 `main_date_label` / `main_time_label`。
  - WiFi 定时器负责刷新 app 顶层时间，同时在 Clock 页面打开时刷新 Clock 页面时间。
- 修复退出 app 层再进入后日期丢失的问题：
  - 原因是 `app_obj` 删除后，其子对象日期和时间标签也会一起删除。
  - 重新进入 `lv_app_page()` 时只重建时间 label，不再重新创建 SNTP 取时任务。
  - SNTP 同步任务只在 WiFi 连接成功后创建一次，避免重复同步和重复创建 LVGL 定时器。
- 当前发现 `main_time_label` 和 `main_date_label` 重叠的原因：
  - `main_time_label` 使用 `lv_obj_align_to(main_date_label, ...)` 时，日期 label 尚未设置文本，LVGL 计算到的日期宽度偏小。
  - 后续应先设置日期文本，再对齐时间 label；定时刷新日期后也应重新对齐时间 label。
- 日志 `Waiting for system time to be set...` 表示 WiFi 已连接，但 SNTP 还没完成校时，不属于 UI bug。
  - 如果该日志短时间内出现后停止，属于正常等待。
  - 如果长期不停止，需要检查 NTP 服务器、DNS 或外网访问，例如 `cn.pool.ntp.org` 是否可达。
- 后续建议验证：
  - 联网成功后 app 顶层能显示日期和时间。
  - 退出 app 层再进入，日期和时间仍能重新显示。
  - 点击主页 Clock 应用不再重启。
  - Clock 页面打开时能显示独立日期和时间。
  - 观察 SNTP 日志，短时间打印 `Waiting for system time to be set...` 属于正常等待。
- 本次未主动执行 `idf.py build`、`idf.py flash` 或烧录，只做了静态检查和代码路径分析。

以下内容基于源工程整理，只记录本轮主要改动点，方便后续回看。

## 1. UI 文件模块拆分

- 将原来集中在 `main/app_ui.c` 中的 UI 逻辑拆分到几个较小文件中，降低单文件混杂度。
- 保留 `main/app_ui.c` 作为主入口和主界面文件，继续放置 `lv_gui_start()`、`lv_main_page()` 等入口逻辑。
- 新增 `main/app_ui_internal.h`，只在 UI 内部 `.c` 文件之间共享必要对象和事件函数声明。
- 新增/整理的 UI 模块：
  - `main/app_ui_music.c`：音乐播放器相关逻辑。
  - `main/app_ui_wifi.c`：WiFi、SNTP 时间显示相关逻辑。
  - `main/app_ui_apps.c`：姿态传感器、SD 卡文件列表、摄像头、蓝牙设置等应用。
- 更新 `main/CMakeLists.txt`，加入拆分后的 `.c` 文件。

## 2. 开机动画改动

- 将开机图片资源改为 `main/assets/img_kidd.c`。
- 在 `img_kidd.c` 中补充/修正 `lv_img_dsc_t img_kidd` 描述符。
- `lv_gui_start()` 中使用 `img_kidd` 创建开机图。
- 原来的旋转动画改成整张图片淡入：
  - 初始透明度为 `LV_OPA_TRANSP`。
  - 通过 LVGL 动画逐步变为 `LV_OPA_COVER`。
- 进入主界面前删除开机图动画，避免动画残留影响后续点击或访问已删除对象。
- 小图不会被强制放大，避免显示发糊。

## 3. SD 卡 MP3 播放

- 音乐播放器改为从 SD 卡 `/sdcard` 根目录扫描音乐文件。
- 只加入 `.mp3` / `.MP3` 文件。
- 支持中文文件名按当前 FATFS UTF-8 链路直接显示和打开。
- 进入播放器时挂载 SD 卡，退出播放器时停止播放器并卸载由播放器挂载的 SD 卡。
- 增加异常状态提示：
  - SD 卡挂载失败。
  - 未找到 MP3 文件。
  - 播放器初始化失败。
- 扩大文件名和路径缓存，适配较长文件名和中文 UTF-8 文件名。

## 4. MP3 播放稳定性优化

- 修复播放器初始化函数命名冲突，避免 `app_ui.h` 中旧声明和音乐模块内部函数类型冲突。
- 恢复音频时钟配置回调，MP3 解码得到采样率、位深、声道后会调用 `bsp_codec_set_fs()` 配置 codec。
- `audio_player_play()` 增加返回值检查，播放失败时关闭文件并打印日志。
- 修复部分 MP3 带 ID3v2 标签时解码报 `ERR_MP3_INVALID_FRAMEHEADER (-6)` 的问题：
  - 识别到 ID3v2 标签后跳过标签区，从真实音频帧开始解码。
  - 遇到无效帧头时推进读指针，避免卡在同一位置重复报错。
- 该修复位于 `managed_components/chmorgan__esp-audio-player/audio_mp3.cpp`，该文件被单独纳入 Git 跟踪。

## 5. 音乐播放器 UI 调整

- 播放/暂停按钮不再依赖 LVGL `CHECKABLE` 状态，避免默认 checked 状态导致背景颜色异常。
- 播放按钮状态改为根据播放器回调统一更新：
  - 播放中显示暂停图标。
  - 暂停时显示播放图标。
- 修复播放/暂停按钮背景样式切换：
  - 进入播放器 IDLE 初始状态时，主动设置为播放图标和播放态背景。
  - 播放中显示暂停图标，并使用已有 `style_pause_bg` 橙色背景 `0xf87c30`。
  - 非播放状态显示播放图标，并新增独立 `style_play_bg` 蓝色背景 `0x3080ff`，避免沿用白色通用按钮背景导致图标不明显。
  - 状态切换时先移除播放态、暂停态和通用背景样式，再添加当前状态对应样式，避免 LVGL 默认主题颜色或旧样式残留。
- 下拉框中文列表使用 `font_alipuhui20`。
- 下拉框右侧展开符号使用 `LV_SYMBOL_DOWN`，并单独使用 Montserrat 字体显示，避免符号变成方框。
- 当前确认可用版本已打 Git 标签：`sd-mp3`。

## 6. 心知天气获取与主页天气显示

- 新增 `main/weather_api.c` / `main/weather_api.h`，用于通过 `esp_http_client` 请求心知天气 API。
- 当前使用实时天气接口 `WEATHER_NOW_URL`：
  - 获取 `results[0].location.name` 作为城市。
  - 从 `results[0].location.path` 中解析省份，例如 `温州,温州,浙江,中国` 解析出 `浙江`。
  - 获取 `results[0].now.text` 作为天气文字。
  - 获取 `results[0].now.temperature` 作为当前温度。
  - 保留终端打印：省份、城市、天气、温度、更新时间。
- 首页新增天气显示区域：
  - 在 `lv_main_page()` 的 `//联网获取天气` 位置调用 `main_weather_create()`。
  - 使用 `main_weather_update()` 在天气请求成功后更新主页显示。
  - 主页右侧天气布局拆成多个 label，避免大字体温度和单位、提示文字重叠：
    - `main_weather_temp_label`：大号温度数字。
    - `main_weather_unit_label`：小号 `°C` 单位。
    - `main_weather_now_label`：天气文字。
    - `main_weather_location_label`：省份和城市。
- `WEATHER_DAILY_URL` 和三天天气预报解析代码暂时保留但注释掉：
  - 暂不显示最近三天预报。
  - 注释中保留了按照 `cJSON_Parse()`、`cJSON_GetObjectItem()`、`cJSON_GetArrayItem()` 解析 `results[0].daily[0]` 的教学说明。
- 当前天气只在联网并完成 SNTP 对时后获取一次，没有做定时刷新。
- 本次未主动执行 `idf.py build`、`idf.py flash` 或烧录。
- 

## 10. 天气 App 三日预报显示

- 本次记录的是主页天气图标进入后的天气 App 页面，不是主页右上角实时天气显示。
- 天气 App 页面由 `main/app_ui_base.c` 中的 `weather_event_handler()` 创建：
  - 顶层仍使用 `icon_in_obj` 作为 320x240 的应用页面容器。
  - 顶部创建 `weather_title` 标题栏，标题文字为“近日天气”。
  - 标题栏左侧增加返回按钮，点击后调用 `btn_weather_back_cb()` 删除 `icon_in_obj` 返回主界面。
  - 返回时应将 `weather_cont` 置空，避免保留已删除 LVGL 子对象指针。
- 天气 App 内容区使用 `weather_cont`：
  - 通过 `lv_obj_create(icon_in_obj)` 创建容器。
  - `lv_obj_set_size(weather_cont, 300, 185)`，用于在 320x240 页面中避开 40px 标题栏，并容纳三张 `280x55` 天气卡片。
  - 使用 `lv_obj_set_flex_flow(weather_cont, LV_FLEX_FLOW_COLUMN)` 设置纵向 flex 布局。
  - 使用 `lv_obj_set_style_pad_row(weather_cont, 5, 0)` 控制卡片间距。
- 每天天气使用一个 card 显示：
  - card 由 `weather_card_create()` 创建，尺寸为 `280 x 55`。
  - 第一行显示日期和白天到夜间天气，例如 `5/22  中雨 → 小雨`。
  - 第二行显示温度范围、降雨量、湿度，例如 `22~25℃  降雨5.9  湿95%`。
  - 日期显示通过 `weather_make_short_date()` 从 `YYYY-MM-DD` 转为 `M/D`，让卡片内容更紧凑。
- 三日天气数据结构定义在 `main/app_ui.h`：
  - `WEATHER_DAILY_MAX_DAYS` 固定为 3。
  - `weather_daily_info_t` 保存 `date`、`text_day`、`text_night`、`high`、`low`、`rainfall`、`humidity`。
- `main/weather_api.c` 中启用三日天气获取：
  - 使用已有 `WEATHER_DAILY_URL` 请求心知天气三日预报接口。
  - 在 `weather_fetch_task()` 中，获取实时天气后继续请求三日天气。
  - `weather_daily_parse_and_update()` 解析 `results[0].daily[0..2]`。
  - 解析字段包括日期、白天天气、夜间天气、最高温、最低温、降雨量、湿度。
  - 降雨量使用 `atof()` 转为数字后通过 `snprintf(..., "%.1f", ...)` 保留 1 位小数。
- 天气 App 页面刷新逻辑：
  - 解析成功后调用三日天气 UI 更新函数，把数据复制到 `main/app_ui_base.c` 的静态缓存数组。
  - 缓存存在的原因是天气数据可能先于天气 App 页面获取；页面未打开时先保存数据，用户打开后直接渲染。
  - `weather_daily_render()` 负责清空 `weather_cont` 并重新创建三张天气卡片。
  - 如果 `main_weather_daily_count == 0`，页面显示“天气数据获取中”。
- 命名注意：
  - 主页实时天气继续由 `main_weather_update()` 更新。
  - 三日预报属于天气 App 页面，更新函数更合适命名为 `app_weather_daily_update()`，避免误解为主页天气。
- 本次未主动执行 `idf.py build`、`idf.py flash` 或烧录。

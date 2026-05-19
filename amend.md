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

# MiniConsole

![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue.svg) ![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2.6-green.svg) ![LVGL](https://img.shields.io/badge/LVGL-v8.3.11-orange.svg) ![Status](https://img.shields.io/badge/status-WIP-yellow.svg)

MiniConsole 是一个基于 ESP32-S3、ESP-IDF 和 LVGL 的迷你控制台 / 桌面终端项目。

本项目基于[立创实战派S3](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/)二次开发，主要用于探索 ESP32-S3 在图形界面、音频播放、无线连接和外设控制等场景下的综合应用。通过集成多种硬件外设与软件协议，MiniConsole 目前已具备以下核心能力：
- **智能桌面终端**：内置精美的 LVGL 交互界面，支持启动动画、状态栏信息展示及应用中心多入口导航。
- **本地多媒体中心**：支持直接读取 SD 卡中的 MP3 文件并解码播放，具备后台无缝播放、UI 生命周期解耦以及硬件级一键启停控制功能。
- **环境信息看板**：集成 Wi-Fi 网络模块与 SNTP 自动时钟同步，通过接入心知天气 API 实时获取并渲染当前天气与多日预报。
- **无线外设控制器**：内置经过优化的 BLE HID 协议栈，可将设备化身为蓝牙外设（如键盘、鼠标）无缝连接到手机或 PC 端。
- **丰富的硬件扩展**：预留了摄像头实时画面预览、传感器交互等外设驱动与应用入口，具备极强的二次开发潜力。

## 📚 目录

- [项目背景](#项目背景)
- [核心技术要点](#核心技术要点)
- [功能列表](#功能列表)
- [硬件平台](#硬件平台)
- [软件环境](#软件环境)
- [目录结构](#目录结构)
- [安装方式](#安装方式)
- [使用说明](#使用说明)
- [FAQ](#faq)
- [致谢](#致谢)

## 🧭 项目背景

MiniConsole 的目标是将 ESP32-S3 的显示、触摸、音频、无线连接和外设能力整合到一个小型可交互设备中，形成一个可运行、可扩展的嵌入式终端工程。

项目当前更偏向完整设备固件，而不是通用组件库。代码与具体硬件、引脚配置和外设型号绑定较强，移植到其他开发板时需要根据硬件重新适配。

## ✨ 核心技术要点

- **LVGL UI 模块化设计**：将原本庞杂的单文件 UI 逻辑拆分至多个子模块（如音乐、Wi-Fi、应用中心等），通过统一的内部头文件共享事件与状态，大幅降低了代码耦合度，提高了可扩展性与可维护性。
- **后台音乐播放与按键全局控制**：实现了 UI 生命周期与底层音频播放解耦。退出播放器界面后音乐仍可无缝后台播放；引入了硬件按键（IO0 / BOOT 键）轮询与消抖逻辑，实现全局随时一键播放/暂停控制。
- **SD 卡共享挂载机制**：在底层挂载逻辑中引入引用计数管理，解决音乐后台播放期间，其他应用（如文件浏览器）再次访问 SD 卡时引发的挂载冲突与重启问题。
- **蓝牙 BLE HID 深度调优**：针对 ESP32-S3 内部 DRAM 紧张问题，优化了蓝牙协议栈的内存分配（启用 PSRAM 与动态内存分配）；完善了 BLE HID 的广播启停状态机与资源回收逻辑，彻底解决了多次进出蓝牙页面导致的广播失败与内存泄漏。
- **天气 API 接入与 JSON 解析**：封装了 `esp_http_client` 接入心知天气 API，完整解析并展示实时天气与三日预报数据；设计了数据缓存机制，确保网络请求与 LVGL 界面异步渲染完美配合。
- **稳定的 SNTP 同步与时间显示**：实现了基于 Wi-Fi 连接的系统时钟同步；独立出主界面顶层状态栏与时钟 App 的时间渲染逻辑，彻底解决了时间标签覆盖、对象重复创建或指针失效导致的系统异常。
- **工程结构规范化**：基于 ESP-IDF v5.2.6，使用 `sdkconfig.defaults` 管理核心配置，结合 Component Manager 自动处理依赖，保障了跨设备编译的一致性。

## ✅ 功能列表

- [x] 开机界面与开机音效
- [x] LVGL 主界面与应用中心
- [x] 独立时钟应用与 SNTP 时间同步
- [x] 天气信息获取与三日预报显示
- [X] 日常秒表功能
- [x] 姿态传感器（运动监测）应用
- [x] 音乐播放界面（SD 卡 MP3 播放）
- [x] SD 卡文件列表浏览应用
- [x] 摄像头画面预览应用
- [x] Wi-Fi 扫描与连接应用
- [x] BLE HID 控制（蓝牙键盘/鼠标）应用
- [x] 全局物理按键交互（一键播放/暂停音乐）


## 🔧 硬件平台

本项目基于**立创开发板**（ESP32-S3），其核心硬件参数与外设型号如下表所示。移植到其他硬件时，需要重点检查 BSP、引脚定义、屏幕驱动、触摸驱动和音频 Codec 配置。

| 类别 | 型号 | 核心参数说明 |
| :--- | :--- | :--- |
| **主控模组** | ESP32-S3-WROOM-1-N16R8 | Xtensa 32位 LX7 双核，主频 240MHz<br>内置 SRAM 512KB，外置 PSRAM 8MB，外置 FLASH 16MB |
| **显示屏** | ST7789 | 2.0寸 IPS 全视角，分辨率 320x240，SPI 接口 |
| **触摸屏** | FT6336 | 电容触摸，I2C 接口 |
| **姿态传感器** | QMI8658 | 三轴加速+三轴陀螺仪，I2C 接口 |
| **音频外设** | ES8311 / ES7210 | 包含音频 DAC (播放) 及音频 ADC，支持本地硬件解码输出 |
| **外部交互** | 物理按键 / 扩展接口 | 提供用户自定义按键（如 IO0 播放控制）及传感器接口（I2C/UART等） |

## 🧩 软件环境

| 项目 | 版本 |
| --- | --- |
| ESP-IDF | v5.2.6 |
| LVGL | v8.3.11 |
| Target | esp32s3 |
| Build System | CMake / ESP-IDF Component Manager |

主要依赖见：

- `main/idf_component.yml`
- `dependencies.lock`

## 📁 目录结构

```text
MiniConsole/
├── docs/                    # 项目文档与截图
├── main/
│   ├── assets/              # LVGL 图片、字体等资源
│   ├── bt/                  # BLE HID 相关代码
│   ├── app_ui.c             # UI 入口与界面逻辑
│   ├── app_ui.h
│   ├── app_ui_internal.h    # UI 内部共享头文件
│   ├── app_ui_apps.c        # 应用入口相关界面
│   ├── app_ui_base.c        # 基础 UI 页面
│   ├── app_ui_music.c       # 音乐播放界面
│   ├── app_ui_wifi.c        # Wi-Fi 扫描与连接界面
│   ├── esp32_s3_szp.c       # 板级外设初始化与驱动封装
│   ├── esp32_s3_szp.h
│   ├── main.c               # app_main 入口
│   ├── weather_api.c        # 天气接口请求与解析
│   ├── weather_api.h
│   ├── idf_component.yml    # ESP-IDF 组件依赖
│   └── sword.pcm            # 音频资源
├── CMakeLists.txt
├── dependencies.lock
├── partitions.csv
├── sdkconfig.defaults
└── README.md
```

## 🚀 安装方式

### 1. 获取项目代码

请使用 `git` 命令克隆本项目：

```bash
git clone https://github.com/你的用户名/MiniConsole.git
```

### 2. 安装 ESP-IDF

请先安装 ESP-IDF v5.2.6，并确认命令行环境已经正确加载 ESP-IDF。

```bash
idf.py --version
```

### 3. 进入项目目录

```bash
cd MiniConsole
```

### 4. 设置目标芯片

```bash
idf.py set-target esp32s3
```

### 5. 构建固件

```bash
idf.py build
```

### 6. 烧录与串口监视

请将 `COMx` 替换为你的实际串口号。

```bash
idf.py -p COMx flash monitor
```

Linux / macOS 可参考：

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## 🕹️ 使用说明

1. 烧录完成后，设备上电启动。
2. 系统初始化 NVS、I2C、屏幕、触摸、音频 Codec 等外设。
3. 启动页显示后进入主界面。
4. 在主界面中通过图标进入不同功能页面。
5. Wi-Fi 页面可扫描附近热点并输入密码连接（2.4G）。
6. 天气功能需要先在代码中配置自己的天气 API 地址或 Key。
7. 音乐、BLE HID、摄像头等功能根据当前 UI 入口使用。

天气接口配置位置：

```c
// main/weather_api.c
#define WEATHER_NOW_URL "ENTER_YOUR_WEATHER_API_KEY"
#define WEATHER_DAILY_URL "ENTER_YOUR_WEATHER_API_KEY"
```

## ❓ FAQ

### Q1: ESP32-S3 内部 DRAM 资源紧张，本项目如何优化内存分配以防止 OOM？

**A:** 在同时运行 LVGL 渲染、Wi-Fi 与蓝牙协议栈时，内存极易见底。本项目通过开启 `CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST=y` 与 `CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY=y`，将蓝牙的较大缓存对象优先分配至外部 PSRAM 中，并使用动态环境内存技术，大幅缓解了内部 DRAM 的压力，确保了系统的稳定运行。

### Q2: 如何解决在后台播放音乐时，启动其他依赖 SD 卡的应用导致的挂载冲突？

**A:** 原生 `bsp_sdcard_mount` 多次调用会导致冲突或初始化失败。我们在底层的挂载与卸载逻辑中引入了**引用计数（Reference Counting）机制**。当音乐处于后台播放时，SD 卡已挂载；此时其它 App 再次请求挂载仅增加计数，退出时仅减少计数，从而实现了全局安全复用唯一的 FATFS 实例。

### Q3: 为什么退出音乐播放器界面后音乐仍能继续播放，且系统不会崩溃？

**A:** 本项目在架构上将“UI 渲染生命周期”与“底层音频处理任务”进行了彻底解耦。退出应用仅销毁 LVGL 的视觉 Widget 对象，而音频解码器和 I2S 提交流程在独立任务中维持运行。配合安全的指针清空与空检测机制，使得音乐能在后台无缝播放，并可通过外部物理按键（如 IO0 轮询）直接向音频任务发送控制信号。

### Q4: 为什么在未配网（Wi-Fi 未连接）的情况下，界面的时间布局会出现重叠？

**A:** 主界面状态栏的时间与日期控件高度依赖于通过 SNTP 获取的系统时间戳。在设备尚未联网或 NTP 同步未完成时，LVGL 由于文本内容为空或默认长度异常，在动态对齐（`lv_obj_align_to`）时会产生坐标计算偏差。当前设计中，系统在检测到 SNTP 授时成功后会自动触发回调，重新赋值文本并计算对象宽度以完成修正布局。

### Q5: 为什么进入蓝牙 BLE HID 模式时有时会报错 `advertising start failed`？

**A:** 该故障多发于未开启蓝牙连接特性的情况。在 ESP-IDF 5.2.6 中，使用可连接的 `ADV_TYPE_IND` 广播需要底层开启 `CONFIG_BT_CTRL_BLE_MASTER=y` 编译选项。本项目已在 sdkconfig 默认配置中修正此参数，并进一步完善了进出蓝牙应用时的广播启停状态机与 Bluedroid 资源的回收释放逻辑，有效杜绝了多次启停造成的广播失败。


## 🙏 致谢

- ESP-IDF
- LVGL
- ESP-IDF Component Manager 生态中的相关组件
- 立创开源硬件平台

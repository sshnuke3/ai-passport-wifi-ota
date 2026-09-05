<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# AI Passport — Wi-Fi OTA 装载器

[FoloToy/ai-passport](https://github.com/FoloToy/ai-passport) 的修改版，给 AI Passport 加了一条
Wi-Fi OTA 通道。第一次 USB 烧入后，后续玩法可以走无线推送，不再需要插线。

**这不是一个玩法，而是一份系统级固件修改**（分区表 + 底层 launcher），用来作为后续 OTA
玩法的基础。

## 它做什么

- 在 `0x360000` 增加 `ota_0` 应用分区（3 MB），在 `0x310000` 增加 `otadata` 分区（8 KB）。
  `factory`、`cardid` (`0x356000`)、`recovery` (`0x700000`) 一个未动。
- 菜单新增 `Wireless Update` 入口。选中即写 RTC 标志后软重启，进 OTA 模式。
- OTA 模式**不初始化 LVGL**，起一个 AP (`AIPassport-OTA` / 密码 `updateme`) 和一个
  `esp_http_server`，监听 `http://192.168.4.1/`。
- 通过 `/update` POST 固件镜像，写入 `ota_0`，切换启动分区，重启后跑新固件。
- 任意时刻长按 `UP` 5 秒仍可进官方 Recovery——硬件级唯一救砖通道，本分支保留。

## 为什么做这个

官方升级路径是电脑 USB + Web Serial 工具。开发完一个新玩法没法无线刷到设备上，每次都得
插线。这条分支在不依赖任何闭源组件的前提下补上这个能力。

**没有破坏官方小程序 BLE 安装契约**——合并镜像仍能通过 `tools/verify_firmware.py` 门禁。

## 分区布局

```
nvs         0x009000  24 KB
phy_init   0x00f000   4 KB
factory    0x010000   3 MB   ← 主 app 槽，本分支装在这里
otadata    0x310000   8 KB   ← 新增，OTA 簿记
cardid     0x356000  16 KB   ← 受保护，设备身份
ota_0      0x360000   3 MB   ← 新增，OTA 目标槽
recovery   0x700000   1 MB   ← 受保护，官方 Recovery
```

## 构建

```bash
. /path/to/esp-idf/export.sh
cd ai-passport
idf.py set-target esp32c3          # 首次
idf.py build
idf merge-bin -o FoloToy-AI-Passport-full.bin
python tools/verify_firmware.py build
```

最后一行是官方兼容门禁。通过后 `FoloToy-AI-Passport-full.bin` 才是可刷的产物。

> Windows Git Bash 下，`idf merge-bin -o build/<file>.bin` 会因命令在 `build/` 目录内执行而报
> `FileNotFoundError`。要么从 `build/` 目录里跑，要么把 `-o` 改成相对文件名。

## 安装

1. **首次必须 USB。** 用[官方网页刷机工具](https://ai-passport.folotoy.cn) 把
   `build/FoloToy-AI-Passport-full.bin` 从 `0x0` 烧入。这一步同时写新分区表、创建 OTA 所需
   的 `otadata`/`ota_0` 槽。
2. 重启后主菜单多出一项 `Wireless Update`。

## 用 OTA 推玩法

1. 卡片里走到 `Wireless Update`，按 `OK`。屏幕显示 AP 名和密码后自动重启进 OTA 模式。
2. 手机连 `AIPassport-OTA` Wi-Fi（密码 `updateme`）。
3. 浏览器打开 `http://192.168.4.1/`，选你编译好的 `FoloToy-AI-Passport-full.bin`，提交。
4. 卡片把镜像写入 `ota_0`，切换启动分区，重启后跑新玩法。

> OTA 推的也是**合并镜像**，和官方工具要求一致。Recovery 和 cardid 永远不会被覆盖。

## 真机安全清单（必须确认）

这条分支开发时手上没设备，下面三项上线前要逐项验：

- [ ] `otadata` 全 `0xFF` 时，bootloader 能否回退到 `factory` 正常启动。
- [ ] 长按 `UP` 5 秒仍能进官方 Recovery——硬件级唯一救砖路径。
- [ ] 工厂复位不会以把 `otadata` 擦坏的方式把卡片变砖。

官方 `recovery_boot_hook` 没动，所以 (2) 理论上不受影响。

## 限制

- AP 只用 WPA2-PSK。任何拿到密码的人都能推固件。别在不信任的环境里启用 OTA。
- OTA 模式关 LVGL 是为了给 HTTP 传输留内存。ESP32-C3 约 400 KB SRAM 是硬约束，推 1.5 MB
  镜像在 802.11b/g 下耗时 20–60 秒。
- 这条分支是**系统级修改**，作者**有意不**上玩法社区——见下文"它不是什么"。

## 它不是什么

- 不是玩法。装上后没有可玩的游戏、工具或互动体验。
- 不是官方功能。FoloToy 没审过也没背书。
- 不是 Recovery 的替代。OTA 失败就走 USB + Recovery。

## 文件改动

```
partitions.csv            — 增加 otadata + ota_0
main/ota.h                — 新增，公共 API
main/ota.c                — 新增，AP + http server + esp_ota 流水线
main/demo_ota_update.c    — 新增，菜单入口触发 OTA 重启
main/demo.h               — 增加 demo_ota_update_* 声明
main/main.c               — 注册 demo + 接入 ota_mode_try_enter()
main/CMakeLists.txt       — 增加 ota.c / demo_ota_update.c，REQUIRES app_update esp_partition esp_http_server
```
// main/ota.h —— 无线更新（Wi-Fi OTA）公共接口。
//
// 设计要点（务必先看 §8 手册）:
//   - 设备起一个 Wi-Fi AP,手机浏览器连上后上传裸固件镜像(app image,0xE9 起)。
//   - 镜像写入 ota_0 分区,完成后 esp_ota_set_boot_partition 并重启。
//   - OTA 模式不初始化 LVGL/Display,把有限的 SRAM 全留给网络栈。C3 约 400 KB。
//   - 触发方式:在菜单选「Wireless Update」→ ota_request_reboot() 写 RTC 标志并软重启。
//
// 已知风险(无设备,需你实测,见手册 §8「必须实测三项」):
//   1. 加 otadata 后 bootloader 是否仍 fallback 到 factory。
//   2. 长按 UP 5 秒进 Recovery 的 hook 是否仍生效(唯一救砖通道)。
//   3. 软重启后 RTC_DATA_ATTR 标志是否保留;若不保留改用 NVS。
#pragma once

#include <stdbool.h>

// 在 app_main 最早期调用。若检测到 OTA 请求标志,进入 OTA 模式并跑到底(不会返回
// 到调用方,内部最终 esp_restart)。返回 true 表示请求已被接管。
bool ota_mode_try_enter(void);

// 请求进入 OTA 模式:写请求标志并软重启。供菜单「Wireless Update」演示页调用。
void ota_request_reboot(void);

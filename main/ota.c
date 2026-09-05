// main/ota.c —— 无线更新实现:Wi-Fi AP + HTTP 上传 + esp_ota 写入 ota_0。
//
// OTA 模式刻意不碰 LVGL / Display / Audio,把 SRAM 全部留给网络栈。ESP32-C3 只有
// 约 400 KB SRAM,同时跑 LVGL + Wi-Fi 会很紧;OTG 模式下关掉 UI 是最稳的选择。
//
// 触发:菜单「Wireless Update」→ ota_request_reboot() 写 RTC 标志 + 软重启 →
// app_main 早期 ota_mode_try_enter() 命中标志 → 本文件 ota_run() 跑到底。
//
// 详见手册 §8「自建无线更新通道」与「必须实测三项」。
#include "ota.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "ota";

// RTC 慢速内存标志:esp_restart() 软重启后保留,断电不保留。
// 若你的板子软重启不保留此标志,改成写 NVS 键(见手册 §8 实测项 3)。
RTC_DATA_ATTR static bool s_ota_requested;

#define OTA_CHUNK     1024
#define OTA_AP_SSID   "AIPassport-OTA"
#define OTA_AP_PASS   "updateme"      // WPA2 要求 >= 8 字符,正好 8 位
#define OTA_HTTP_PORT 80

static esp_ota_handle_t s_ota_handle;
static size_t           s_total;
static size_t           s_written;

static const char *HTML_PAGE =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>AI Passport OTA</title></head>"
    "<body style='font-family:sans-serif;padding:1em'>"
    "<h2>AI Passport 无线刷机</h2>"
    "<p>设备热点: <b>AIPassport-OTA</b> / 密码: <b>updateme</b></p>"
    "<p>上传完整合并镜像 <b>FoloToy-AI-Passport-full.bin</b>(从 0x0 起)。</p>"
    "<input id=f type=file accept='.bin'><br><br>"
    "<button id=b disabled>选择 .bin 后点此上传</button>"
    "<p id=s>等待上传...</p>"
    "<script>"
    "const f=document.getElementById('f'),b=document.getElementById('b'),s=document.getElementById('s');"
    "f.onchange=()=>{b.disabled=!f.files.length;b.textContent=f.files.length?'上传 '+f.files[0].name:'选择文件'};"
    "b.onclick=()=>{const file=f.files[0];if(!file)return;b.disabled=true;"
    "const xhr=new XMLHttpRequest();xhr.open('POST','/update');"
    "xhr.upload.onprogress=e=>{s.textContent='上传 '+(e.loaded/e.total*100).toFixed(0)+'%'};"
    "xhr.onload=()=>s.textContent='完成,设备重启中...';"
    "xhr.onerror=()=>s.textContent='失败,检查网络';"
    "xhr.send(file)};"
    "</script></body></html>";

static esp_err_t ota_begin(void) {
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) {
        ESP_LOGE(TAG, "找不到 ota_0 更新分区");
        return ESP_ERR_NOT_FOUND;
    }
    s_total   = part->size;
    s_written = 0;
    ESP_LOGI(TAG, "OTA 目标分区 %s @ 0x%lx, 大小 %u", part->label,
             (unsigned long)part->address, part->size);
    return esp_ota_begin(part, OTA_SIZE_UNKNOWN, &s_ota_handle);
}

static esp_err_t start_wifi_ap(void) {
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) return err;
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) return err;

    wifi_config_t ap = {
        .ap = {
            .ssid_len   = strlen(OTA_AP_SSID),
            .channel    = 1,
            .max_connection = 1,
            .authmode   = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)ap.ap.ssid, OTA_AP_SSID, sizeof(ap.ap.ssid) - 1);
    strncpy((char *)ap.ap.password, OTA_AP_PASS, sizeof(ap.ap.password) - 1);

    err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err != ESP_OK) return err;
    return esp_wifi_start();
}

static esp_err_t update_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t update_post_handler(httpd_req_t *req) {
    esp_err_t err = ota_begin();
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_begin failed");
        return ESP_OK;
    }

    size_t remaining = req->content_len;
    char *buf = malloc(OTA_CHUNK);
    if (!buf) {
        esp_ota_abort(s_ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "开始接收固件, 长度 %u", (unsigned)remaining);
    while (remaining > 0) {
        int to_recv = remaining > OTA_CHUNK ? OTA_CHUNK : (int)remaining;
        int got = httpd_req_recv(req, buf, to_recv);
        if (got <= 0) {                       // 对端断开或超时
            ESP_LOGE(TAG, "接收中断 (%d)", got);
            free(buf);
            esp_ota_abort(s_ota_handle);
            return ESP_FAIL;
        }
        err = esp_ota_write(s_ota_handle, buf, (size_t)got);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write 失败: %s", esp_err_to_name(err));
            free(buf);
            esp_ota_abort(s_ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_write failed");
            return ESP_OK;
        }
        s_written += (size_t)got;
        remaining -= (size_t)got;
        if ((s_written & 0x3FFFF) == 0 || remaining == 0) {
            ESP_LOGI(TAG, "已写 %u / %u", (unsigned)s_written, (unsigned)s_total);
        }
    }
    free(buf);

    err = esp_ota_end(s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end 失败(镜像校验不过?): %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_end failed");
        return ESP_OK;
    }

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_boot_partition 失败: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "set_boot failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "OTA 完成, 即将重启进入新固件");
    httpd_resp_send(req, "OK - rebooting", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;                           // 不会到达
}

static esp_err_t start_httpd(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size      = 8192;           // C3 上 handler 需处理 recv,给足
    config.max_uri_handlers = 8;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) return err;

    httpd_uri_t get = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = update_get_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t post = {
        .uri      = "/update",
        .method   = HTTP_POST,
        .handler  = update_post_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &get);
    httpd_register_uri_handler(server, &post);
    return ESP_OK;
}

static void ota_run(void) {
    ESP_LOGI(TAG, "启动 OTA 模式:Wi-Fi AP + HTTP 上传服务");
    nvs_flash_init();                        // 失败也不阻塞 Wi-Fi
    if (start_wifi_ap() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi AP 启动失败");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(500));          // 等 AP 起来
    if (start_httpd() != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 服务启动失败");
        return;
    }
    ESP_LOGI(TAG, "OTA 就绪:手机连 AIPassport-OTA,浏览器开 http://192.168.4.1");
    // 阻塞在此:HTTP server 在自己的任务里跑,本任务挂起即可。
    vTaskDelay(portMAX_DELAY);
}

bool ota_mode_try_enter(void) {
    if (!s_ota_requested) return false;
    s_ota_requested = false;
    ESP_LOGI(TAG, "检测到 OTA 请求,进入 OTA 模式(跳过显示初始化)");
    ota_run();
    return true;                             // ota_run 内含 esp_restart,正常不返回
}

void ota_request_reboot(void) {
    s_ota_requested = true;
    ESP_LOGI(TAG, "请求 OTA 模式,即将软重启");
    esp_restart();
}

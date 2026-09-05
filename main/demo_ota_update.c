// main/demo_ota_update.c —— 菜单里的「Wireless Update」入口页。
// 进入即请求重启进入 OTA 模式(见 ota.c)。它本身不做 UI 交互。
#include "demo.h"
#include "ota.h"
#include "ui_pixel.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "demo_ota_update";

static lv_obj_t *s_scr;

void demo_ota_update_enter(void) {
    s_scr = ui_pixel_screen_create("WIRELESS UPDATE");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 12, 54, 216, 190, UI_PAPER);

    lv_obj_t *txt = lv_label_create(panel);
    lv_obj_set_width(txt, 196);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(txt, lv_color_hex(UI_INK), 0);
    lv_label_set_text(txt,
        "即将重启进入无线更新模式。\n\n"
        "手机连热点 AIPassport-OTA\n密码 updateme\n浏览器开 192.168.4.1\n"
        "上传 full.bin 即可刷机");
    lv_obj_align(txt, LV_ALIGN_TOP_LEFT, 4, 6);

    ui_pixel_mascot_create(s_scr, 101, 246);
    lv_screen_load(s_scr);

    ESP_LOGI(TAG, "请求进入 OTA 模式,1 秒后重启");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ota_request_reboot();                  // 内部 esp_restart,不返回
    return;
}

void demo_ota_update_exit(void) {
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
}

void demo_ota_update_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    (void)btn;
    (void)ev;
}

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
    // 双 slot 轮换:把「非当前」的那个分区当写入目标,装载器与玩法互为备份。
    lv_label_set_text_fmt(txt,
        "本次写入: %s\n\n"
        "手机连热点 AIPassport-OTA\n密码 updateme\n浏览器开 192.168.4.1\n\n"
        "上传 app 镜像\nFoloToy-AI-Passport.bin\n(不是 -full.bin)",
        ota_is_running_factory() ? "ota_0" : "factory");
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

// ---------------------------------------------------------------------------
// 「Back to Loader」:把启动分区切回 factory(装载器)并重启。
//
// 双 slot 轮换的另一半。OTA 之后设备跑在 ota_0 的玩法上,玩法固件若不带 OTA 菜单,
// 就再也回不去 —— 这个入口保证随时能回到装载器。已经在装载器上时只是提示。
// ---------------------------------------------------------------------------
static lv_obj_t *s_revert_scr;

void demo_ota_revert_enter(void) {
    s_revert_scr = ui_pixel_screen_create("BACK TO LOADER");
    lv_obj_t *panel = ui_pixel_panel_create(s_revert_scr, 12, 54, 216, 190, UI_PAPER);

    lv_obj_t *txt = lv_label_create(panel);
    lv_obj_set_width(txt, 196);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(txt, lv_color_hex(UI_INK), 0);

    if (ota_is_running_factory()) {
        lv_label_set_text(txt,
            "当前已运行在装载器\n(factory) 分区。\n\n"
            "无需回退。\n\n"
            "长按 OK 返回菜单。");
    } else {
        lv_label_set_text(txt,
            "当前运行: ota_0 玩法\n\n"
            "即将切回装载器(factory)\n并重启。\n\n"
            "切回后无线更新入口仍在。");
    }
    lv_obj_align(txt, LV_ALIGN_TOP_LEFT, 4, 6);

    ui_pixel_mascot_create(s_revert_scr, 101, 246);
    lv_screen_load(s_revert_scr);

    if (ota_is_running_factory()) {
        return;                                  // 等用户长按 OK 返回
    }

    ESP_LOGI(TAG, "1 秒后切回装载器(factory)");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ota_revert_to_factory();                     // 内部 esp_restart,不返回
}

void demo_ota_revert_exit(void) {
    if (s_revert_scr) {
        lv_obj_delete(s_revert_scr);
        s_revert_scr = NULL;
    }
}

void demo_ota_revert_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    (void)btn;
    (void)ev;
}

// main/demo_ota_update.c —— "Wireless Update" menu entry page.
// Entering it arms a reboot into OTA mode (see ota.c). No auto-reboot:
// the user must press OK to confirm, so the on-screen instructions are readable.
#include "demo.h"
#include "ota.h"
#include "ui_pixel.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "demo_ota_update";

static lv_obj_t *s_scr;
static bool s_await_confirm;          // true after enter; OK click triggers reboot

void demo_ota_update_enter(void) {
    s_scr = ui_pixel_screen_create("WIRELESS UPDATE");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 12, 54, 216, 190, UI_PAPER);

    lv_obj_t *txt = lv_label_create(panel);
    lv_obj_set_width(txt, 196);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(txt, lv_color_hex(UI_INK), 0);
    // Two-slot rotation: write to the slot we are NOT running from, so loader
    // and play firmware back each other up.
    lv_label_set_text_fmt(txt,
        "Target: %s\n\n"
        "Join Wi-Fi AP: AIPassport-OTA\npass: updateme\nopen http://192.168.4.1\n\n"
        "Upload app image\nFoloToy-AI-Passport.bin\n(NOT -full.bin)\n\n"
        "Press OK to reboot\ninto update mode",
        ota_is_running_factory() ? "ota_0" : "factory");
    lv_obj_align(txt, LV_ALIGN_TOP_LEFT, 4, 6);

    ui_pixel_mascot_create(s_scr, 101, 246);
    lv_screen_load(s_scr);

    s_await_confirm = true;
    ESP_LOGI(TAG, "Wireless Update armed; waiting for OK to reboot into OTA mode");
}

void demo_ota_update_exit(void) {
    s_await_confirm = false;
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
}

void demo_ota_update_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (!s_await_confirm) return;
    if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        s_await_confirm = false;
        ota_request_reboot();                  // internal esp_restart, does not return
    }
}

// ---------------------------------------------------------------------------
// "Back to Loader": flip the boot partition back to factory (the loader) and
// reboot. The other half of the two-slot rotation. After OTA the device runs
// the ota_0 play firmware; if that firmware lacks an OTA menu you could be
// stuck — this entry guarantees you can always return to the loader. When
// already on the loader it just shows a notice and lets you back out.
// ---------------------------------------------------------------------------
static lv_obj_t *s_revert_scr;
static bool s_revert_await_confirm;

void demo_ota_revert_enter(void) {
    s_revert_scr = ui_pixel_screen_create("BACK TO LOADER");
    lv_obj_t *panel = ui_pixel_panel_create(s_revert_scr, 12, 54, 216, 190, UI_PAPER);

    lv_obj_t *txt = lv_label_create(panel);
    lv_obj_set_width(txt, 196);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(txt, lv_color_hex(UI_INK), 0);

    if (ota_is_running_factory()) {
        s_revert_await_confirm = false;
        lv_label_set_text(txt,
            "Already on loader\n(factory) partition.\n\n"
            "Nothing to revert.\n\n"
            "Hold OK to return.");
    } else {
        s_revert_await_confirm = true;
        lv_label_set_text(txt,
            "Now running: ota_0 play\n\n"
            "Will switch boot back\nto loader (factory)\nand reboot.\n\n"
            "Press OK to revert");
    }
    lv_obj_align(txt, LV_ALIGN_TOP_LEFT, 4, 6);

    ui_pixel_mascot_create(s_revert_scr, 101, 246);
    lv_screen_load(s_revert_scr);

    ESP_LOGI(TAG, "Back to Loader: %s",
             s_revert_await_confirm ? "armed, waiting for OK" : "already on loader");
}

void demo_ota_revert_exit(void) {
    s_revert_await_confirm = false;
    if (s_revert_scr) {
        lv_obj_delete(s_revert_scr);
        s_revert_scr = NULL;
    }
}

void demo_ota_revert_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (!s_revert_await_confirm) return;
    if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        s_revert_await_confirm = false;
        ota_revert_to_factory();               // internal esp_restart, does not return
    }
}

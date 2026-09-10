// main/demo_pet.c -- 精灵球电子宠物 (Sprite Pet) for FoloToy AI Passport.
//
// Hardware target (see components/bsp/include/bsp_pins.h):
//   ESP32-C3, 8MB Flash (no PSRAM), ST7789P3 240x320, 3 ADC buttons (GPIO0),
//   CW2017 battery on I2C0. LVGL v9, 16-bit color.
//
// Notes / limitations (build success != hardware validation):
//   * No CJK font is compiled into this firmware, so all UI text is English/ASCII.
//   * Pet state is persisted to NVS. Because the board has no battery-backed RTC,
//     stats do NOT decay while powered off -- the pet "pauses" and resumes from
//     the last saved values on next boot. Within a session, stats decay over time.
//   * The Pokeball shell is drawn with LVGL primitives (circles/arcs/lines);
//     exact pixel layout may need on-device tweaks.

#include <stdio.h>
#include "demo.h"
#include "ui_pixel.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "pet";

// LVGL v9 dropped pet_set_visible(); use the HIDDEN flag instead.
static void pet_set_visible(lv_obj_t *o, int v)
{
    if (v) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else   lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// ---------------------------------------------------------------------------
// Pet model
// ---------------------------------------------------------------------------
typedef enum { STAGE_EGG = 0, STAGE_BABY, STAGE_TEEN, STAGE_ADULT, STAGE_STAR } stage_t;
typedef enum { MOOD_HAPPY, MOOD_OK, MOOD_SAD, MOOD_SLEEP, MOOD_SICK, MOOD_EGG } mood_t;

#define STAT_FOOD 0
#define STAT_FUN  1
#define STAT_NRG  2
#define STAT_CLN  3
#define STAT_N    4

#define PET_NS      "pet"
#define DI_FOOD     22   // seconds per -1
#define DI_FUN      16
#define DI_NRG      26
#define DI_CLN      34
#define SAVE_EVERY  15   // ticks between NVS saves

typedef struct {
    uint8_t stat[STAT_N];   // 0..100, higher = better
    uint16_t exp;           // care experience
    uint8_t hatched;        // 0 = egg, 1 = hatched
} pet_t;

static pet_t s_pet = { {80, 80, 80, 80}, 0, 0 };

// ---------------------------------------------------------------------------
// UI objects
// ---------------------------------------------------------------------------
static lv_obj_t *s_scr;
static lv_obj_t *s_aura;
static lv_obj_t *s_ball;
static lv_obj_t *s_redtop;
static lv_obj_t *s_divider;
static lv_obj_t *s_btn_out;
static lv_obj_t *s_btn_in;
static lv_obj_t *s_egg;
static lv_obj_t *s_egg_line;
static lv_obj_t *s_eye_l, *s_eye_r;          // open eyes (circles)
static lv_obj_t *s_eye_l_line, *s_eye_r_line;// closed / X eyes (lines)
static lv_obj_t *s_cheek_l, *s_cheek_r;
static lv_obj_t *s_mouth_arc;                // smile / frown
static lv_obj_t *s_mouth_line;               // neutral / sleep / sick
static lv_obj_t *s_z1, *s_z2;                // sleep "z"
static lv_obj_t *s_title;                    // stage + level
static lv_obj_t *s_bat;                      // battery %
static lv_obj_t *s_stat[STAT_N];             // bars
static lv_obj_t *s_stat_lbl[STAT_N];
static lv_obj_t *s_menu;                      // action menu panel
static lv_obj_t *s_menu_txt;
static lv_obj_t *s_hint;
static lv_obj_t *s_fb;                        // feedback flash

static lv_timer_t *s_tick;

// layout constants (screen 240x320)
#define CX 120
#define CY 132
#define D  146

static const char *ACTIONS[] = { "FEED", "PLAY", "SLEEP", "WASH", "CURE" };
#define ACT_N (sizeof(ACTIONS) / sizeof(ACTIONS[0]))
static int s_mode;        // 0 = view, 1 = menu
static int s_sel;         // selected action

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static lv_obj_t *mk_circle(lv_obj_t *parent, int cx, int cy, int d, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, cx - d / 2, cy - d / 2);
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static lv_obj_t *mk_block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static void persist(void)
{
    nvs_handle_t h;
    if (nvs_open(PET_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "food", s_pet.stat[STAT_FOOD]);
    nvs_set_u8(h, "fun",  s_pet.stat[STAT_FUN]);
    nvs_set_u8(h, "nrg",  s_pet.stat[STAT_NRG]);
    nvs_set_u8(h, "cln",  s_pet.stat[STAT_CLN]);
    nvs_set_u16(h, "exp", s_pet.exp);
    nvs_set_u8(h, "hatch", s_pet.hatched);
    nvs_commit(h);
    nvs_close(h);
}

static void load(void)
{
    nvs_handle_t h;
    if (nvs_open(PET_NS, NVS_READONLY, &h) != ESP_OK) return;
    nvs_get_u8(h, "food", &s_pet.stat[STAT_FOOD]);
    nvs_get_u8(h, "fun",  &s_pet.stat[STAT_FUN]);
    nvs_get_u8(h, "nrg",  &s_pet.stat[STAT_NRG]);
    nvs_get_u8(h, "cln",  &s_pet.stat[STAT_CLN]);
    nvs_get_u16(h, "exp", &s_pet.exp);
    nvs_get_u8(h, "hatch", &s_pet.hatched);
    nvs_close(h);
}

static stage_t stage_of(void)
{
    if (!s_pet.hatched) return STAGE_EGG;
    if (s_pet.exp < 30)  return STAGE_BABY;
    if (s_pet.exp < 100) return STAGE_TEEN;
    if (s_pet.exp < 300) return STAGE_ADULT;
    return STAGE_STAR;
}

static const char *stage_name(stage_t s)
{
    switch (s) {
        case STAGE_EGG:  return "EGG";
        case STAGE_BABY: return "BABY";
        case STAGE_TEEN: return "TEEN";
        case STAGE_ADULT:return "ADULT";
        case STAGE_STAR: return "STAR";
    }
    return "?";
}

static uint32_t stage_aura(stage_t s)
{
    switch (s) {
        case STAGE_BABY: return 0x9AD0FF;
        case STAGE_TEEN: return 0x82BE2D;
        case STAGE_ADULT:return 0xFFB23E;
        case STAGE_STAR: return 0xC77DFF;
        case STAGE_EGG:
        default:         return 0xDDDDDD;
    }
}

static mood_t mood_of(void)
{
    if (!s_pet.hatched) return MOOD_EGG;
    if (s_pet.stat[STAT_FOOD] == 0 || s_pet.stat[STAT_FUN] == 0 ||
        s_pet.stat[STAT_NRG] == 0  || s_pet.stat[STAT_CLN] == 0) return MOOD_SICK;
    int avg = (s_pet.stat[STAT_FOOD] + s_pet.stat[STAT_FUN] +
               s_pet.stat[STAT_NRG]  + s_pet.stat[STAT_CLN]) / 4;
    if (s_pet.stat[STAT_NRG] < 25) return MOOD_SLEEP;
    if (avg < 35) return MOOD_SAD;
    if (avg < 70) return MOOD_OK;
    return MOOD_HAPPY;
}

static void clamp_stats(void)
{
    for (int i = 0; i < STAT_N; i++) {
        if (s_pet.stat[i] > 100) s_pet.stat[i] = 100;
        if ((int)s_pet.stat[i] < 0) s_pet.stat[i] = 0;
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
static void set_bar(lv_obj_t *bar, int v)
{
    lv_bar_set_value(bar, v, LV_ANIM_OFF);
    uint32_t c = v > 50 ? 0x55C15A : (v > 20 ? 0xFFC23E : 0xE43B2F);
    lv_obj_set_style_bg_color(bar, lv_color_hex(c), LV_PART_INDICATOR);
}

static void refresh_face(mood_t m)
{
    bool egg = (m == MOOD_EGG);
    pet_set_visible(s_egg, egg);
    pet_set_visible(s_egg_line, egg);
    pet_set_visible(s_eye_l, !egg);
    pet_set_visible(s_eye_r, !egg);
    pet_set_visible(s_cheek_l, !egg && m == MOOD_HAPPY);
    pet_set_visible(s_cheek_r, !egg && m == MOOD_HAPPY);
    pet_set_visible(s_z1, !egg && m == MOOD_SLEEP);
    pet_set_visible(s_z2, !egg && m == MOOD_SLEEP);

    // eyes
    bool open = (m == MOOD_HAPPY || m == MOOD_OK || m == MOOD_SAD);
    pet_set_visible(s_eye_l, open && !egg);
    pet_set_visible(s_eye_r, open && !egg);
    pet_set_visible(s_eye_l_line, !open && !egg);
    pet_set_visible(s_eye_r_line, !open && !egg);

    // mouth
    if (m == MOOD_HAPPY || m == MOOD_SAD) {
        pet_set_visible(s_mouth_arc, true);
        pet_set_visible(s_mouth_line, false);
        if (m == MOOD_HAPPY) {
            lv_arc_set_angles(s_mouth_arc, 200, 340);   // smile
            lv_obj_set_style_arc_color(s_mouth_arc, lv_color_hex(UI_INK), 0);
        } else {
            lv_arc_set_angles(s_mouth_arc, 20, 160);    // frown
            lv_obj_set_style_arc_color(s_mouth_arc, lv_color_hex(UI_INK), 0);
        }
    } else {
        pet_set_visible(s_mouth_arc, false);
        pet_set_visible(s_mouth_line, !egg);
        if (m == MOOD_SICK) {
            static lv_point_precise_t p[4] = {{CX-18, CY+36},{CX-8, CY+30},{CX+2, CY+36},{CX+14, CY+30}};
            lv_line_set_points(s_mouth_line, p, 4);
        } else { // OK / SLEEP / default neutral
            static lv_point_precise_t p[2] = {{CX-12, CY+38},{CX+12, CY+38}};
            lv_line_set_points(s_mouth_line, p, 2);
        }
    }
}

static void refresh(void)
{
    mood_t m = mood_of();
    stage_t st = stage_of();

    lv_obj_set_style_bg_color(s_aura, lv_color_hex(stage_aura(st)), 0);
    lv_label_set_text_fmt(s_title, "%s  LV%d", stage_name(st), s_pet.exp / 10);

    int b = bsp_battery_soc();
    if (b < 0) lv_label_set_text(s_bat, "BAT --");
    else       lv_label_set_text_fmt(s_bat, "BAT %d%%", b);

    for (int i = 0; i < STAT_N; i++) set_bar(s_stat[i], s_pet.stat[i]);

    refresh_face(m);

    if (s_mode == 1) {
        char buf[96];
        int n = 0;
        for (int i = 0; i < ACT_N; i++) {
            n += snprintf(buf + n, sizeof(buf) - n, "%s%s\n",
                          i == s_sel ? "> " : "  ", ACTIONS[i]);
        }
        lv_label_set_text(s_menu_txt, buf);
    }
}

static void fb_timer_cb(lv_timer_t *t)
{
    lv_obj_t *fb = (lv_obj_t *)lv_timer_get_user_data(t);
    pet_set_visible(fb, false);
    lv_timer_del(t);
}

static void ball_bounce_cb(void *o, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)o, v);
}

static void flash(const char *msg)
{
    lv_label_set_text(s_fb, msg);
    pet_set_visible(s_fb, true);
    lv_timer_create(fb_timer_cb, 1200, s_fb);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
static void do_action(int a)
{
    if (!s_pet.hatched) {
        if (a != 0) { flash("EGG! FEED"); refresh(); return; }
        s_pet.hatched = 1;
        s_pet.stat[STAT_FOOD] = 100;
        s_pet.exp = 0;
        flash("HATCH!");
        persist();
        refresh();
        return;
    }

    bool sick = (s_pet.stat[STAT_FOOD] == 0 || s_pet.stat[STAT_FUN] == 0 ||
                 s_pet.stat[STAT_NRG] == 0  || s_pet.stat[STAT_CLN] == 0);

    switch (a) {
        case 0: // FEED
            s_pet.stat[STAT_FOOD] = (uint8_t)(s_pet.stat[STAT_FOOD] + 18);
            s_pet.stat[STAT_CLN]  = (uint8_t)(s_pet.stat[STAT_CLN] - 5);
            flash("YUM!");
            break;
        case 1: // PLAY
            s_pet.stat[STAT_FUN]  = (uint8_t)(s_pet.stat[STAT_FUN] + 18);
            s_pet.stat[STAT_NRG]  = (uint8_t)(s_pet.stat[STAT_NRG] - 10);
            s_pet.stat[STAT_FOOD] = (uint8_t)(s_pet.stat[STAT_FOOD] - 5);
            flash("WHEEE!");
            break;
        case 2: // SLEEP
            s_pet.stat[STAT_NRG]  = (uint8_t)(s_pet.stat[STAT_NRG] + 25);
            s_pet.stat[STAT_FOOD] = (uint8_t)(s_pet.stat[STAT_FOOD] - 5);
            flash("Zzz...");
            break;
        case 3: // WASH
            s_pet.stat[STAT_CLN]  = (uint8_t)(s_pet.stat[STAT_CLN] + 25);
            flash("SPARKLE!");
            break;
        case 4: // CURE
            if (sick) {
                for (int i = 0; i < STAT_N; i++)
                    if (s_pet.stat[i] == 0) s_pet.stat[i] = 35;
                s_pet.exp = (uint16_t)(s_pet.exp + 3);
                flash("CURED!");
            } else {
                flash("HEALTHY!");
            }
            break;
    }
    clamp_stats();
    if (!sick) s_pet.exp = (uint16_t)(s_pet.exp + 5);
    if (s_pet.exp > 999) s_pet.exp = 999;
    persist();
    refresh();
}

// ---------------------------------------------------------------------------
// Decay tick
// ---------------------------------------------------------------------------
static int s_acc[STAT_N] = {0,0,0,0};
static int s_ticks;

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    int iv[STAT_N] = {DI_FOOD, DI_FUN, DI_NRG, DI_CLN};
    for (int i = 0; i < STAT_N; i++) {
        s_acc[i]++;
        if (s_acc[i] >= iv[i]) {
            s_acc[i] = 0;
            if (s_pet.stat[i] > 0) s_pet.stat[i]--;
        }
    }
    s_ticks++;
    if (s_ticks % SAVE_EVERY == 0) persist();
    refresh();
}

// ---------------------------------------------------------------------------
// Build screen
// ---------------------------------------------------------------------------
static void build_view(void)
{
    s_scr = ui_pixel_screen_create("PIX");

    // aura + ball
    s_aura = mk_circle(s_scr, CX, CY, D + 14, 0xDDDDDD);
    s_ball = mk_circle(s_scr, CX, CY, D, 0xFFFFFF);
    lv_obj_set_style_border_color(s_ball, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_border_width(s_ball, 5, 0);

    // red top half (thick arc fills the upper semicircle)
    s_redtop = lv_arc_create(s_scr);
    lv_obj_set_size(s_redtop, D, D);
    lv_obj_align_to(s_redtop, s_ball, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_mode(s_redtop, LV_ARC_MODE_NORMAL);
    lv_arc_set_angles(s_redtop, 180, 360);
    lv_arc_set_bg_angles(s_redtop, 180, 360);
    lv_obj_set_style_arc_opa(s_redtop, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_redtop, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_redtop, lv_color_hex(UI_RED), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_redtop, D, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_redtop, false, LV_PART_INDICATOR);
    lv_obj_set_style_opa(s_redtop, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(s_redtop, LV_OBJ_FLAG_CLICKABLE);

    // divider line (Pokeball split) + center button
    s_divider = mk_block(s_scr, CX - D/2, CY - 2, D, 4, UI_INK);
    s_btn_out = mk_circle(s_scr, CX, CY, 26, UI_INK);
    s_btn_in  = mk_circle(s_scr, CX, CY, 16, 0xFFFFFF);

    // creature (face lives in lower white area)
    s_eye_l = mk_circle(s_scr, CX - 22, CY + 16, 12, UI_INK);
    s_eye_r = mk_circle(s_scr, CX + 22, CY + 16, 12, UI_INK);
    s_cheek_l = mk_circle(s_scr, CX - 36, CY + 32, 9, 0xFF9FB0);
    s_cheek_r = mk_circle(s_scr, CX + 36, CY + 32, 9, 0xFF9FB0);

    s_mouth_arc = lv_arc_create(s_scr);
    lv_obj_set_size(s_mouth_arc, 44, 44);
    lv_obj_align_to(s_mouth_arc, s_ball, LV_ALIGN_CENTER, 0, 30);
    lv_arc_set_bg_angles(s_mouth_arc, 0, 360);
    lv_obj_set_style_arc_opa(s_mouth_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_mouth_arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_mouth_arc, lv_color_hex(UI_INK), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_mouth_arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_mouth_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_opa(s_mouth_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(s_mouth_arc, LV_OBJ_FLAG_CLICKABLE);

    s_mouth_line = lv_line_create(s_scr);
    lv_obj_set_style_line_width(s_mouth_line, 4, 0);
    lv_obj_set_style_line_color(s_mouth_line, lv_color_hex(UI_INK), 0);
    static lv_point_precise_t pl[4];
    lv_line_set_points(s_mouth_line, pl, 4);

    // closed / X eyes
    s_eye_l_line = lv_line_create(s_scr);
    s_eye_r_line = lv_line_create(s_scr);
    lv_obj_set_style_line_width(s_eye_l_line, 4, 0);
    lv_obj_set_style_line_width(s_eye_r_line, 4, 0);
    lv_obj_set_style_line_color(s_eye_l_line, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_line_color(s_eye_r_line, lv_color_hex(UI_INK), 0);
    static lv_point_precise_t el[2] = {{CX-30, CY+10},{CX-14, CY+22}};
    static lv_point_precise_t er[2] = {{CX+14, CY+10},{CX+30, CY+22}};
    lv_line_set_points(s_eye_l_line, el, 2);
    lv_line_set_points(s_eye_r_line, er, 2);

    // egg (shown when not hatched)
    s_egg = mk_circle(s_scr, CX, CY + 8, 56, 0xFFE08A);
    lv_obj_set_style_border_color(s_egg, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_border_width(s_egg, 3, 0);
    s_egg_line = lv_line_create(s_scr);
    lv_obj_set_style_line_width(s_egg_line, 3, 0);
    lv_obj_set_style_line_color(s_egg_line, lv_color_hex(UI_INK), 0);
    static lv_point_precise_t eggp[6] = {{CX-26,CY-2},{CX-14,CY+6},{CX-2,CY-2},
                                 {CX+10,CY+6},{CX+22,CY-2},{CX+30,CY+4}};
    lv_line_set_points(s_egg_line, eggp, 6);

    // sleep z's
    s_z1 = ui_pixel_label(s_scr, "z", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_z1, CX + 40, CY - 18);
    s_z2 = ui_pixel_label(s_scr, "Z", &lv_font_montserrat_20, UI_INK);
    lv_obj_set_pos(s_z2, CX + 54, CY - 40);

    // top-right battery + title
    s_bat = ui_pixel_label(s_scr, "BAT --", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_bat, 178, 14);
    s_title = ui_pixel_label(s_scr, "EGG", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_title, 12, 300);

    // stat bars
    static const char *sl[STAT_N] = { "F", "Fn", "E", "C" };
    for (int i = 0; i < STAT_N; i++) {
        int x = 12 + i * 58;
        s_stat_lbl[i] = ui_pixel_label(s_scr, sl[i], &lv_font_montserrat_14, UI_INK);
        lv_obj_set_pos(s_stat_lbl[i], x, 214);
        s_stat[i] = lv_bar_create(s_scr);
        lv_obj_set_size(s_stat[i], 50, 12);
        lv_obj_set_pos(s_stat[i], x, 232);
        lv_bar_set_range(s_stat[i], 0, 100);
        lv_obj_set_style_bg_color(s_stat[i], lv_color_hex(0xDDDDDD), LV_PART_MAIN);
        lv_obj_set_style_radius(s_stat[i], 3, 0);
    }

    // action menu (hidden in view)
    s_menu = mk_block(s_scr, 8, 210, 224, 96, 0xF4F4EA);
    lv_obj_set_style_border_color(s_menu, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_border_width(s_menu, 3, 0);
    s_menu_txt = ui_pixel_label(s_menu, "", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_style_text_line_space(s_menu_txt, 4, 0);
    lv_obj_set_pos(s_menu_txt, 14, 10);
    pet_set_visible(s_menu, false);

    s_hint = ui_pixel_label(s_scr, "OK: Menu   UP/DN: Pet", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_hint, 12, 252);

    s_fb = ui_pixel_label(s_scr, "", &lv_font_montserrat_20, UI_RED);
    lv_obj_set_pos(s_fb, 70, 168);
    pet_set_visible(s_fb, false);

    refresh();
    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------------------
// Demo interface
// ---------------------------------------------------------------------------
void demo_pet_enter(void)
{
    nvs_flash_init();   // safe to call repeatedly; nvs_open() below works if already open
    load();
    s_mode = 0;
    s_sel = 0;
    s_ticks = 0;
    for (int i = 0; i < STAT_N; i++) s_acc[i] = 0;
    bsp_display_backlight(100);
    build_view();
    s_tick = lv_timer_create(tick_cb, 1000, NULL);
}

void demo_pet_exit(void)
{
    if (s_tick) { lv_timer_del(s_tick); s_tick = NULL; }
    persist();
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
}

void demo_pet_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;

    if (s_mode == 0) {                         // VIEW
        if (btn == BSP_BTN_OK) {
            s_mode = 1;
            s_sel = 0;
            pet_set_visible(s_menu, true);
            pet_set_visible(s_hint, false);
            for (int i = 0; i < STAT_N; i++) pet_set_visible(s_stat[i], false);
            for (int i = 0; i < STAT_N; i++) pet_set_visible(s_stat_lbl[i], false);
            refresh();
        } else {                               // UP/DN: pet reacts
            // tiny bounce of the ball
            int y = lv_obj_get_y(s_ball);
            lv_anim_t a; lv_anim_init(&a);
            lv_anim_set_var(&a, s_ball);
            lv_anim_set_exec_cb(&a, ball_bounce_cb);
            lv_anim_set_values(&a, y, y - 6);
            lv_anim_set_duration(&a, 100);
            lv_anim_set_playback_duration(&a, 130);
            lv_anim_set_path_cb(&a, lv_anim_path_step);
            lv_anim_start(&a);
        }
    } else {                                  // MENU
        if (btn == BSP_BTN_UP)   { s_sel = (s_sel + ACT_N - 1) % ACT_N; refresh(); }
        if (btn == BSP_BTN_DOWN) { s_sel = (s_sel + 1) % ACT_N;          refresh(); }
        if (btn == BSP_BTN_OK) {
            int a = s_sel;
            // back to view, then act
            s_mode = 0;
            pet_set_visible(s_menu, false);
            pet_set_visible(s_hint, true);
            for (int i = 0; i < STAT_N; i++) pet_set_visible(s_stat[i], true);
            for (int i = 0; i < STAT_N; i++) pet_set_visible(s_stat_lbl[i], true);
            do_action(a);
        }
    }
}

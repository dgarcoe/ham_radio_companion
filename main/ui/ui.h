/*
 * Ham Radio Companion - UI Header
 * Replaces SquareLine-generated sensor UI
 */

#ifndef _HAM_RADIO_UI_H
#define _HAM_RADIO_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

/* ---------- Screen 1: Clock ---------- */
void ui_event_screen_clock(lv_event_t *e);
extern lv_obj_t *ui_screen_clock;
extern lv_obj_t *ui_wifi_st_1;
extern lv_obj_t *ui_utc_label;
extern lv_obj_t *ui_local_time;
extern lv_obj_t *ui_date;
extern lv_obj_t *ui_callsign_label;
extern lv_obj_t *ui_grid_label;
extern lv_obj_t *ui_prop_mini_sfi;
extern lv_obj_t *ui_prop_mini_a;
extern lv_obj_t *ui_prop_mini_k;
extern lv_obj_t *ui_clock_dot;

/* ---------- Screen 2: Propagation ---------- */
void ui_event_screen_propagation(lv_event_t *e);
extern lv_obj_t *ui_screen_propagation;
extern lv_obj_t *ui_wifi_st_2;
extern lv_obj_t *ui_time2;
extern lv_obj_t *ui_prop_sfi_val;
extern lv_obj_t *ui_prop_a_val;
extern lv_obj_t *ui_prop_k_val;
extern lv_obj_t *ui_prop_band_table;
extern lv_obj_t *ui_prop_updated;
extern lv_obj_t *ui_prop_dot;

/* Band condition labels: [band_index][0=day, 1=night] */
#define PROP_NUM_BANDS  4
extern lv_obj_t *ui_prop_band_day[PROP_NUM_BANDS];
extern lv_obj_t *ui_prop_band_night[PROP_NUM_BANDS];

/* ---------- Screen 3: DX Cluster ---------- */
void ui_event_screen_dx_cluster(lv_event_t *e);
extern lv_obj_t *ui_screen_dx_cluster;
extern lv_obj_t *ui_wifi_st_3;
extern lv_obj_t *ui_time3;
extern lv_obj_t *ui_dx_filter_label;
extern lv_obj_t *ui_dx_source_label;
extern lv_obj_t *ui_dx_spot_list;
extern lv_obj_t *ui_dx_dot;

/* ---------- Screen 4: Settings ---------- */
void ui_event_screen_settings(lv_event_t *e);
extern lv_obj_t *ui_screen_settings;
extern lv_obj_t *ui_wifi_st_4;
extern lv_obj_t *ui_time4;
extern lv_obj_t *ui_settings_dot;

/* Ham radio settings widgets */
extern lv_obj_t *ui_callsign_ta;
extern lv_obj_t *ui_grid_ta;
extern lv_obj_t *ui_dx_source_dd;
extern lv_obj_t *ui_telnet_host_ta;
extern lv_obj_t *ui_telnet_port_ta;
extern lv_obj_t *ui_alert_en_cb;
extern lv_obj_t *ui_alert_sound_cb;
extern lv_obj_t *ui_alert_bands_label;
extern lv_obj_t *ui_alert_call_ta;
extern lv_obj_t *ui_settings_kb;

/* ---------- Sub-Screens (kept from original) ---------- */
extern lv_obj_t *ui_screen_display;
extern lv_obj_t *ui_wifi_st_5;
void ui_event_back1(lv_event_t *e);
extern lv_obj_t *ui_back1;
extern lv_obj_t *ui_display_title;
extern lv_obj_t *ui_brighness;
extern lv_obj_t *ui_brighness_cfg;
extern lv_obj_t *ui_brighness_title;
extern lv_obj_t *ui_brighness_icon_1;
extern lv_obj_t *ui_brighness_icon_2;
extern lv_obj_t *ui_screen_always_on;
extern lv_obj_t *ui_screen_always_on_title;
void ui_event_screen_always_on_cfg(lv_event_t *e);
extern lv_obj_t *ui_screen_always_on_cfg;
extern lv_obj_t *ui_turn_off_after_time;
extern lv_obj_t *ui_after;
void ui_event_sleep_mode_time_cfg(lv_event_t *e);
extern lv_obj_t *ui_turn_off_after_time_cfg;
void ui_event_display_keyboard(lv_event_t *e);
extern lv_obj_t *ui_display_keyboard;

extern lv_obj_t *ui_screen_date_time;
extern lv_obj_t *ui_wifi_st_6;
void ui_event_back2(lv_event_t *e);
extern lv_obj_t *ui_back2;
extern lv_obj_t *ui_date_time_title;
extern lv_obj_t *ui_time_format;
extern lv_obj_t *ui_time_format_title;
extern lv_obj_t *ui_time_format_cfg;
extern lv_obj_t *ui_auto_update;
extern lv_obj_t *ui_auto_update_title;
void ui_event_auto_update_cfg(lv_event_t *e);
extern lv_obj_t *ui_auto_update_cfg;
extern lv_obj_t *ui_date_time;
extern lv_obj_t *ui_time_zone;
extern lv_obj_t *ui_zone_auto_update_cfg;
extern lv_obj_t *ui_time_zone_title;
extern lv_obj_t *ui_time_zone_num_cfg;
extern lv_obj_t *ui_utc_tile;
extern lv_obj_t *ui_time_zone_sign_cfg_;
extern lv_obj_t *ui_daylight_title;
extern lv_obj_t *ui_daylight_cfg;
extern lv_obj_t *ui_manual_setting_title;
extern lv_obj_t *ui_date_cfg;
extern lv_obj_t *ui_hour_cfg;
extern lv_obj_t *ui_min_cfg;
extern lv_obj_t *ui_sec_cfg;
extern lv_obj_t *ui_time_label1;
extern lv_obj_t *ui_time_label2;
extern lv_obj_t *ui_time_save;

extern lv_obj_t *ui_screen_wifi;
extern lv_obj_t *ui_wifi_st_7;
extern lv_obj_t *ui_wifi_title;
void ui_event_back3(lv_event_t *e);
extern lv_obj_t *ui_back3;

extern lv_obj_t *ui_screen_factory;
extern lv_obj_t *ui_factory_resetting_title;

/* ---------- Image Declarations ---------- */
LV_IMG_DECLARE(ui_img_wifi_disconet_png);
LV_IMG_DECLARE(ui_img_setting_png);
LV_IMG_DECLARE(ui_img_wifi_setting_png);
LV_IMG_DECLARE(ui_img_display_png);
LV_IMG_DECLARE(ui_img_time_png);
LV_IMG_DECLARE(ui_img_back_png);
LV_IMG_DECLARE(ui_img_high_light_png);
LV_IMG_DECLARE(ui_img_low_light_png);
LV_IMG_DECLARE(ui_img_background_png);
LV_IMG_DECLARE(ui_img_lock_png);
LV_IMG_DECLARE(ui_img_wifi_1_png);
LV_IMG_DECLARE(ui_img_wifi_2_png);
LV_IMG_DECLARE(ui_img_wifi_3_png);

/* ---------- Font Declarations ---------- */
LV_FONT_DECLARE(ui_font_font0);
LV_FONT_DECLARE(ui_font_font1);
LV_FONT_DECLARE(ui_font_font2);
LV_FONT_DECLARE(ui_font_font3);
LV_FONT_DECLARE(ui_font_font4);

void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif

#ifndef VIEW_DATA_H
#define VIEW_DATA_H

#include "config.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

enum start_screen{
    SCREEN_SENSECAP_LOG,
    SCREEN_WIFI_CONFIG,
};

/* ---------- WiFi Data ---------- */
#define WIFI_SCAN_LIST_SIZE  15

struct view_data_wifi_st
{
    bool   is_connected;
    bool   is_connecting;
    bool   is_network;
    char   ssid[32];
    int8_t rssi;
};

struct view_data_wifi_config
{
    char    ssid[32];
    uint8_t password[64];
    bool    have_password;
};

struct view_data_wifi_item
{
    char   ssid[32];
    bool   auth_mode;
    int8_t rssi;
};

struct view_data_wifi_list
{
    bool  is_connect;
    struct view_data_wifi_item  connect;
    uint16_t cnt;
    struct view_data_wifi_item aps[WIFI_SCAN_LIST_SIZE];
};

struct view_data_wifi_connet_ret_msg
{
    uint8_t ret;
    char    msg[64];
};

/* ---------- Display Data ---------- */
struct view_data_display
{
    int   brightness;
    bool  sleep_mode_en;
    int   sleep_mode_time_min;
};

/* ---------- Time Config ---------- */
struct view_data_time_cfg
{
    bool    time_format_24;
    bool    auto_update;
    time_t  time;
    bool    set_time;
    bool    auto_update_zone;
    int8_t  zone;
    bool    daylight;
}__attribute__((packed));

/* ---------- Ham Radio Config ---------- */
#define MAX_CALLSIGN_LEN    12
#define MAX_GRID_LEN        8
#define MAX_DX_HOST_LEN     64
#define MAX_ALERT_PATTERN   32
#define MAX_DX_SPOTS        50
#define NUM_HF_BANDS        8  /* 80,60,40,30,20,17,15,10 */

struct view_data_ham_config {
    char callsign[MAX_CALLSIGN_LEN];
    char grid[MAX_GRID_LEN];
    uint8_t dx_source;        /* 0=HamQTH HTTP, 1=Telnet, 2=Both */
    char dx_telnet_host[MAX_DX_HOST_LEN];
    uint16_t dx_telnet_port;
    char hamqth_user[32];
    char hamqth_pass[32];
};

/* ---------- Propagation Data ---------- */
enum band_condition {
    BAND_POOR = 0,
    BAND_FAIR = 1,
    BAND_GOOD = 2
};

struct view_data_propagation {
    int sfi;
    int a_index;
    int k_index;
    int sunspots;
    char solar_wind[16];
    char mag_field[16];
    char signal_noise[16];
    enum band_condition bands[NUM_HF_BANDS][2]; /* [band][0=day, 1=night] */
    char band_names[NUM_HF_BANDS][8];
    bool valid;
    time_t last_update;
};

/* ---------- DX Cluster Spot ---------- */
struct view_data_dx_spot {
    char dx_call[16];
    char spotter[16];
    float frequency;       /* kHz */
    char comment[32];
    time_t time;
    bool alert_match;
};

struct view_data_dx_list {
    struct view_data_dx_spot spots[MAX_DX_SPOTS];
    uint16_t count;
    bool connected;
};

/* ---------- Alert Config ---------- */
struct view_data_alert_config {
    bool enabled;
    uint8_t bands_mask;    /* bitmask: bit0=80m, bit1=60m, ..., bit7=10m */
    char callsign_pattern[MAX_ALERT_PATTERN];
};

/* ---------- Events ---------- */
enum {
    VIEW_EVENT_SCREEN_START = 0,
    VIEW_EVENT_TIME,

    VIEW_EVENT_WIFI_ST,

    VIEW_EVENT_WIFI_LIST,
    VIEW_EVENT_WIFI_LIST_REQ,
    VIEW_EVENT_WIFI_CONNECT,
    VIEW_EVENT_WIFI_CONNECT_RET,
    VIEW_EVENT_WIFI_CFG_DELETE,

    VIEW_EVENT_TIME_CFG_UPDATE,
    VIEW_EVENT_TIME_CFG_APPLY,

    VIEW_EVENT_DISPLAY_CFG,
    VIEW_EVENT_BRIGHTNESS_UPDATE,
    VIEW_EVENT_DISPLAY_CFG_APPLY,

    VIEW_EVENT_SHUTDOWN,
    VIEW_EVENT_FACTORY_RESET,
    VIEW_EVENT_SCREEN_CTRL,

    /* Ham radio events */
    VIEW_EVENT_PROPAGATION_DATA,
    VIEW_EVENT_DX_SPOT_LIST,
    VIEW_EVENT_DX_NEW_SPOT,
    VIEW_EVENT_DX_ALERT,
    VIEW_EVENT_HAM_CONFIG_UPDATE,
    VIEW_EVENT_HAM_CONFIG_APPLY,
    VIEW_EVENT_ALERT_CONFIG_UPDATE,
    VIEW_EVENT_ALERT_CONFIG_APPLY,
    VIEW_EVENT_DX_REFRESH_REQ,
    VIEW_EVENT_DX_CONNECT_STATUS,
    VIEW_EVENT_CITY,

    VIEW_EVENT_ALL,
};

#ifdef __cplusplus
}
#endif

#endif

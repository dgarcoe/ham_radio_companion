#include "indicator_alert.h"
#include "indicator_ham_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <string.h>
#include <ctype.h>

#define BUZZER_GPIO    19

static const char *TAG = "alert";

/* Band frequency ranges in kHz */
static const struct {
    float low;
    float high;
    uint8_t bit;
    const char *name;
} band_ranges[] = {
    { 3500.0f,  3800.0f,  0, "80m" },
    { 5351.0f,  5367.0f,  1, "60m" },
    { 7000.0f,  7300.0f,  2, "40m" },
    {10100.0f, 10150.0f,  3, "30m" },
    {14000.0f, 14350.0f,  4, "20m" },
    {18068.0f, 18168.0f,  5, "17m" },
    {21000.0f, 21450.0f,  6, "15m" },
    {28000.0f, 29700.0f,  7, "10m" },
};
#define NUM_BAND_RANGES  (sizeof(band_ranges) / sizeof(band_ranges[0]))

static void __buzzer_beep(void)
{
    gpio_set_level((gpio_num_t)BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level((gpio_num_t)BUZZER_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level((gpio_num_t)BUZZER_GPIO, 0);
}

static int __freq_to_band_bit(float freq_khz)
{
    for (int i = 0; i < (int)NUM_BAND_RANGES; i++) {
        if (freq_khz >= band_ranges[i].low && freq_khz <= band_ranges[i].high) {
            return band_ranges[i].bit;
        }
    }
    return -1;
}

static bool __callsign_matches(const char *callsign, const char *pattern)
{
    if (pattern[0] == '\0') {
        return true; /* empty pattern matches all */
    }

    /* Case-insensitive prefix match */
    int plen = (int)strlen(pattern);
    int clen = (int)strlen(callsign);
    if (clen < plen) {
        return false;
    }

    for (int i = 0; i < plen; i++) {
        if (toupper((unsigned char)pattern[i]) != toupper((unsigned char)callsign[i])) {
            return false;
        }
    }
    return true;
}

bool indicator_alert_check_spot(struct view_data_dx_spot *spot)
{
    struct view_data_alert_config alert_cfg;
    indicator_alert_config_get(&alert_cfg);

    if (!alert_cfg.enabled) {
        return false;
    }

    /* Check band filter */
    int band_bit = __freq_to_band_bit(spot->frequency);
    if (band_bit < 0) {
        return false; /* frequency not in any ham band */
    }
    if (!(alert_cfg.bands_mask & (1 << band_bit))) {
        return false; /* band not in filter */
    }

    /* Check callsign pattern */
    if (alert_cfg.callsign_pattern[0] != '\0') {
        if (!__callsign_matches(spot->dx_call, alert_cfg.callsign_pattern)) {
            return false;
        }
    }

    return true;
}

static void __alert_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (id == VIEW_EVENT_DX_ALERT) {
        struct view_data_dx_spot *spot = (struct view_data_dx_spot *)event_data;
        ESP_LOGI(TAG, "ALERT! %.1f %s de %s", spot->frequency, spot->dx_call, spot->spotter);
        __buzzer_beep();
    }
}

int indicator_alert_init(void)
{
    /* Init buzzer GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)BUZZER_GPIO, 0);

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle,
                    VIEW_EVENT_BASE, VIEW_EVENT_DX_ALERT,
                    __alert_event_handler, NULL, NULL));

    ESP_LOGI(TAG, "Alert module initialized");
    return 0;
}

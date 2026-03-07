#include "indicator_propagation.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

#define PROPAGATION_URL       "https://www.hamqsl.com/solarxml.php"
#define PROPAGATION_INTERVAL  (15 * 60 * 1000000ULL) /* 15 minutes in microseconds */
#define HTTP_BUF_SIZE         4096

static const char *TAG = "propagation";

static struct view_data_propagation __g_prop_data;
static SemaphoreHandle_t __g_data_mutex;
static esp_timer_handle_t __g_fetch_timer;
static bool __g_wifi_connected = false;

static const char *band_name_list[NUM_HF_BANDS] = {
    "80m-40m", "30m-20m", "17m-15m", "12m-10m",
    "80m-40m", "30m-20m", "17m-15m", "12m-10m"
};

/* Simple XML tag value extraction */
static int __extract_tag_value(const char *xml, const char *tag, char *out, int out_size)
{
    char open_tag[64];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);

    const char *start = strstr(xml, open_tag);
    if (start == NULL) {
        return -1;
    }
    start += strlen(open_tag);

    char close_tag[64];
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);
    const char *end = strstr(start, close_tag);
    if (end == NULL) {
        return -1;
    }

    int len = (int)(end - start);
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, start, (size_t)len);
    out[len] = '\0';
    return 0;
}

/* Extract band condition from XML like: <band name="80m-40m" time="day">Good</band> */
static enum band_condition __parse_band_condition(const char *value)
{
    if (strcasecmp(value, "Good") == 0) {
        return BAND_GOOD;
    } else if (strcasecmp(value, "Fair") == 0) {
        return BAND_FAIR;
    }
    return BAND_POOR;
}

static void __extract_band_data(const char *xml, const char *band_name, const char *time_str,
                                 enum band_condition *out)
{
    char search[128];
    snprintf(search, sizeof(search), "name=\"%s\" time=\"%s\">", band_name, time_str);

    const char *pos = strstr(xml, search);
    if (pos == NULL) {
        *out = BAND_POOR;
        return;
    }
    pos += strlen(search);

    const char *end = strstr(pos, "</band>");
    if (end == NULL) {
        *out = BAND_POOR;
        return;
    }

    char val[16] = {0};
    int len = (int)(end - pos);
    if (len > 15) len = 15;
    memcpy(val, pos, (size_t)len);
    val[len] = '\0';

    *out = __parse_band_condition(val);
}

static void __parse_solar_xml(const char *xml)
{
    struct view_data_propagation prop;
    char buf[32];

    memset(&prop, 0, sizeof(prop));

    if (__extract_tag_value(xml, "solarflux", buf, sizeof(buf)) == 0) {
        prop.sfi = atoi(buf);
    }
    if (__extract_tag_value(xml, "aindex", buf, sizeof(buf)) == 0) {
        prop.a_index = atoi(buf);
    }
    if (__extract_tag_value(xml, "kindex", buf, sizeof(buf)) == 0) {
        prop.k_index = atoi(buf);
    }
    if (__extract_tag_value(xml, "sunspots", buf, sizeof(buf)) == 0) {
        prop.sunspots = atoi(buf);
    }
    __extract_tag_value(xml, "solarwind", prop.solar_wind, sizeof(prop.solar_wind));
    __extract_tag_value(xml, "magneticfield", prop.mag_field, sizeof(prop.mag_field));
    __extract_tag_value(xml, "signalnoise", prop.signal_noise, sizeof(prop.signal_noise));

    /* Band conditions - the N0NBH XML uses grouped bands */
    static const char *band_groups[] = {"80m-40m", "30m-20m", "17m-15m", "12m-10m"};

    for (int i = 0; i < 4; i++) {
        __extract_band_data(xml, band_groups[i], "day", &prop.bands[i][0]);
        __extract_band_data(xml, band_groups[i], "night", &prop.bands[i][1]);
        strncpy(prop.band_names[i], band_groups[i], 7);
        prop.band_names[i][7] = '\0';
    }

    prop.valid = true;
    time(&prop.last_update);

    xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
    memcpy(&__g_prop_data, &prop, sizeof(prop));
    xSemaphoreGive(__g_data_mutex);

    ESP_LOGI(TAG, "Propagation: SFI=%d A=%d K=%d SN=%d",
             prop.sfi, prop.a_index, prop.k_index, prop.sunspots);

    esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_PROPAGATION_DATA,
                      (void *)&prop, sizeof(prop), portMAX_DELAY);
}

static void __fetch_propagation_data(void *arg)
{
    if (!__g_wifi_connected) {
        ESP_LOGI(TAG, "WiFi not connected, skipping fetch");
        return;
    }

    ESP_LOGI(TAG, "Fetching propagation data...");

    char *buf = (char *)heap_caps_malloc(HTTP_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate HTTP buffer");
        return;
    }

    esp_http_client_config_t config = {
        .url = PROPAGATION_URL,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        heap_caps_free(buf);
        return;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        heap_caps_free(buf);
        return;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int total_read = 0;
    int read_len;

    while (total_read < HTTP_BUF_SIZE - 1) {
        read_len = esp_http_client_read(client, buf + total_read, HTTP_BUF_SIZE - 1 - total_read);
        if (read_len <= 0) {
            break;
        }
        total_read += read_len;
    }
    buf[total_read] = '\0';

    int status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP status=%d, read=%d bytes", status, total_read);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status == 200 && total_read > 0) {
        __parse_solar_xml(buf);
    } else {
        ESP_LOGE(TAG, "HTTP fetch failed, status=%d", status);
    }

    heap_caps_free(buf);
}

static void __wifi_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (id == VIEW_EVENT_WIFI_ST) {
        struct view_data_wifi_st *p_st = (struct view_data_wifi_st *)event_data;
        bool was_connected = __g_wifi_connected;
        __g_wifi_connected = p_st->is_network;

        if (__g_wifi_connected && !was_connected) {
            ESP_LOGI(TAG, "WiFi connected, triggering propagation fetch");
            __fetch_propagation_data(NULL);
        }
    }
}

int indicator_propagation_init(void)
{
    __g_data_mutex = xSemaphoreCreateMutex();
    memset(&__g_prop_data, 0, sizeof(__g_prop_data));

    const esp_timer_create_args_t timer_args = {
        .callback = __fetch_propagation_data,
        .arg = NULL,
        .name = "prop_fetch"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &__g_fetch_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(__g_fetch_timer, PROPAGATION_INTERVAL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle,
                    VIEW_EVENT_BASE, VIEW_EVENT_WIFI_ST,
                    __wifi_event_handler, NULL, NULL));

    ESP_LOGI(TAG, "Propagation module initialized");
    return 0;
}

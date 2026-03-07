#include "indicator_dxcluster.h"
#include "indicator_ham_config.h"
#include "indicator_alert.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define DX_FETCH_INTERVAL   (60 * 1000000ULL)  /* 1 minute */
#define HAMQTH_URL          "https://www.hamqth.com/dxc_csv.php?limit=30"
#define HTTP_BUF_SIZE       8192
#define TELNET_BUF_SIZE     2048
#define TELNET_TASK_STACK   8192

static const char *TAG = "dxcluster";

static struct view_data_dx_list __g_dx_list;
static SemaphoreHandle_t __g_data_mutex;
static esp_timer_handle_t __g_fetch_timer;
static bool __g_wifi_connected = false;
static int __g_telnet_sock = -1;
static TaskHandle_t __g_telnet_task = NULL;

static void __dx_list_clear(void)
{
    xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
    memset(&__g_dx_list, 0, sizeof(__g_dx_list));
    xSemaphoreGive(__g_data_mutex);
}

static void __dx_list_add_spot(struct view_data_dx_spot *spot)
{
    xSemaphoreTake(__g_data_mutex, portMAX_DELAY);

    /* Shift existing spots down, newest first */
    if (__g_dx_list.count < MAX_DX_SPOTS) {
        memmove(&__g_dx_list.spots[1], &__g_dx_list.spots[0],
                sizeof(struct view_data_dx_spot) * __g_dx_list.count);
        __g_dx_list.count++;
    } else {
        memmove(&__g_dx_list.spots[1], &__g_dx_list.spots[0],
                sizeof(struct view_data_dx_spot) * (MAX_DX_SPOTS - 1));
    }
    memcpy(&__g_dx_list.spots[0], spot, sizeof(struct view_data_dx_spot));

    xSemaphoreGive(__g_data_mutex);
}

static void __notify_view(void)
{
    xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
    struct view_data_dx_list list_copy;
    memcpy(&list_copy, &__g_dx_list, sizeof(list_copy));
    xSemaphoreGive(__g_data_mutex);

    esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_DX_SPOT_LIST,
                      (void *)&list_copy, sizeof(list_copy), portMAX_DELAY);
}

/* Parse HamQTH CSV format: callsign,freq,date,time,spotter,comment,lotw,eqsl,continent,band,country */
static void __parse_hamqth_csv(const char *csv)
{
    const char *line = csv;
    int count = 0;

    __dx_list_clear();

    while (line != NULL && *line != '\0' && count < MAX_DX_SPOTS) {
        const char *next_line = strchr(line, '\n');

        char line_buf[256];
        int line_len;
        if (next_line != NULL) {
            line_len = (int)(next_line - line);
        } else {
            line_len = (int)strlen(line);
        }
        if (line_len > 255) line_len = 255;
        if (line_len < 5) {
            line = (next_line != NULL) ? next_line + 1 : NULL;
            continue;
        }

        memcpy(line_buf, line, (size_t)line_len);
        line_buf[line_len] = '\0';

        struct view_data_dx_spot spot;
        memset(&spot, 0, sizeof(spot));

        /* Parse CSV fields using strtok on copy */
        char *saveptr = NULL;
        char *tok;

        /* Field 1: DX callsign */
        tok = strtok_r(line_buf, ",", &saveptr);
        if (tok != NULL) {
            strncpy(spot.dx_call, tok, sizeof(spot.dx_call) - 1);
        }

        /* Field 2: Frequency */
        tok = strtok_r(NULL, ",", &saveptr);
        if (tok != NULL) {
            spot.frequency = (float)atof(tok);
        }

        /* Field 3: Date (skip) */
        tok = strtok_r(NULL, ",", &saveptr);

        /* Field 4: Time */
        tok = strtok_r(NULL, ",", &saveptr);
        if (tok != NULL) {
            /* Parse HH:MMz or similar format */
            int hh = 0, mm = 0;
            if (sscanf(tok, "%d:%d", &hh, &mm) == 2) {
                time_t now;
                time(&now);
                struct tm tm;
                gmtime_r(&now, &tm);
                tm.tm_hour = hh;
                tm.tm_min = mm;
                tm.tm_sec = 0;
                spot.time = mktime(&tm);
            }
        }

        /* Field 5: Spotter */
        tok = strtok_r(NULL, ",", &saveptr);
        if (tok != NULL) {
            strncpy(spot.spotter, tok, sizeof(spot.spotter) - 1);
        }

        /* Field 6: Comment */
        tok = strtok_r(NULL, ",", &saveptr);
        if (tok != NULL) {
            strncpy(spot.comment, tok, sizeof(spot.comment) - 1);
        }

        if (spot.dx_call[0] != '\0' && spot.frequency > 0.0f) {
            spot.alert_match = indicator_alert_check_spot(&spot);
            __dx_list_add_spot(&spot);

            if (spot.alert_match) {
                esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_DX_ALERT,
                                  (void *)&spot, sizeof(spot), portMAX_DELAY);
            }
            count++;
        }

        line = (next_line != NULL) ? next_line + 1 : NULL;
    }

    ESP_LOGI(TAG, "Parsed %d DX spots from HamQTH", count);
    __notify_view();
}

static void __fetch_hamqth_spots(void *arg)
{
    if (!__g_wifi_connected) {
        return;
    }

    ESP_LOGI(TAG, "Fetching DX spots from HamQTH...");

    char *buf = (char *)heap_caps_malloc(HTTP_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate HTTP buffer");
        return;
    }

    esp_http_client_config_t config = {
        .url = HAMQTH_URL,
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

    esp_http_client_fetch_headers(client);
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
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status == 200 && total_read > 0) {
        __parse_hamqth_csv(buf);
    } else {
        ESP_LOGE(TAG, "HamQTH fetch failed, status=%d", status);
    }

    heap_caps_free(buf);
}

/* Parse telnet DX spot line: "DX de SPOTTER:     FREQ  DXCALL       comment            TIME" */
static void __parse_telnet_spot(const char *line)
{
    if (strncmp(line, "DX de ", 6) != 0) {
        return;
    }

    struct view_data_dx_spot spot;
    memset(&spot, 0, sizeof(spot));

    const char *p = line + 6;

    /* Extract spotter (ends with ':') */
    const char *colon = strchr(p, ':');
    if (colon == NULL) return;
    int spotter_len = (int)(colon - p);
    if (spotter_len > 15) spotter_len = 15;
    memcpy(spot.spotter, p, (size_t)spotter_len);
    spot.spotter[spotter_len] = '\0';

    p = colon + 1;
    /* Skip whitespace */
    while (*p == ' ') p++;

    /* Extract frequency */
    spot.frequency = (float)atof(p);
    while (*p != ' ' && *p != '\0') p++;
    while (*p == ' ') p++;

    /* Extract DX callsign */
    const char *call_start = p;
    while (*p != ' ' && *p != '\0') p++;
    int call_len = (int)(p - call_start);
    if (call_len > 15) call_len = 15;
    memcpy(spot.dx_call, call_start, (size_t)call_len);
    spot.dx_call[call_len] = '\0';
    while (*p == ' ') p++;

    /* Rest is comment (up to time at end) */
    strncpy(spot.comment, p, sizeof(spot.comment) - 1);
    /* Trim trailing whitespace and time */
    int clen = (int)strlen(spot.comment);
    while (clen > 0 && (spot.comment[clen - 1] == '\n' || spot.comment[clen - 1] == '\r' || spot.comment[clen - 1] == ' ')) {
        spot.comment[--clen] = '\0';
    }

    time(&spot.time);

    if (spot.dx_call[0] != '\0' && spot.frequency > 0.0f) {
        spot.alert_match = indicator_alert_check_spot(&spot);
        __dx_list_add_spot(&spot);

        ESP_LOGI(TAG, "Telnet spot: %.1f %s de %s", spot.frequency, spot.dx_call, spot.spotter);

        if (spot.alert_match) {
            esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_DX_ALERT,
                              (void *)&spot, sizeof(spot), portMAX_DELAY);
        }
        __notify_view();
    }
}

static void __telnet_task(void *pvParameters)
{
    char *buf = (char *)heap_caps_malloc(TELNET_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate telnet buffer");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        if (!__g_wifi_connected) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        struct view_data_ham_config cfg;
        indicator_ham_config_get(&cfg);

        if (cfg.dx_source == 0) {
            /* HTTP only mode, no telnet */
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        ESP_LOGI(TAG, "Connecting to telnet DX cluster: %s:%d", cfg.dx_telnet_host, cfg.dx_telnet_port);

        struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };
        struct addrinfo *res = NULL;
        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%d", cfg.dx_telnet_port);

        int err = getaddrinfo(cfg.dx_telnet_host, port_str, &hints, &res);
        if (err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed for %s", cfg.dx_telnet_host);
            if (res != NULL) freeaddrinfo(res);
            vTaskDelay(pdMS_TO_TICKS(30000));
            continue;
        }

        __g_telnet_sock = socket(res->ai_family, res->ai_socktype, 0);
        if (__g_telnet_sock < 0) {
            ESP_LOGE(TAG, "Socket creation failed");
            freeaddrinfo(res);
            vTaskDelay(pdMS_TO_TICKS(30000));
            continue;
        }

        if (connect(__g_telnet_sock, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "Socket connect failed");
            close(__g_telnet_sock);
            __g_telnet_sock = -1;
            freeaddrinfo(res);
            vTaskDelay(pdMS_TO_TICKS(30000));
            continue;
        }
        freeaddrinfo(res);

        ESP_LOGI(TAG, "Telnet connected!");

        bool connected_status = true;
        esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_DX_CONNECT_STATUS,
                          (void *)&connected_status, sizeof(connected_status), portMAX_DELAY);

        /* Send login callsign */
        vTaskDelay(pdMS_TO_TICKS(2000));
        char login[32];
        snprintf(login, sizeof(login), "%s\r\n", cfg.callsign);
        send(__g_telnet_sock, login, strlen(login), 0);

        /* Set receive timeout */
        struct timeval tv;
        tv.tv_sec = 60;
        tv.tv_usec = 0;
        setsockopt(__g_telnet_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        /* Read and parse incoming spots */
        char line_buf[256];
        int line_pos = 0;

        while (__g_wifi_connected) {
            int len = recv(__g_telnet_sock, buf, TELNET_BUF_SIZE - 1, 0);
            if (len < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue; /* timeout, just retry */
                }
                ESP_LOGE(TAG, "Telnet recv error: %d", errno);
                break;
            }
            if (len == 0) {
                ESP_LOGI(TAG, "Telnet connection closed");
                break;
            }

            for (int i = 0; i < len; i++) {
                if (buf[i] == '\n') {
                    line_buf[line_pos] = '\0';
                    if (line_pos > 10) {
                        __parse_telnet_spot(line_buf);
                    }
                    line_pos = 0;
                } else if (buf[i] != '\r' && line_pos < 255) {
                    line_buf[line_pos++] = buf[i];
                }
            }
        }

        close(__g_telnet_sock);
        __g_telnet_sock = -1;

        connected_status = false;
        esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_DX_CONNECT_STATUS,
                          (void *)&connected_status, sizeof(connected_status), portMAX_DELAY);

        ESP_LOGI(TAG, "Telnet disconnected, will retry in 30s");
        vTaskDelay(pdMS_TO_TICKS(30000));
    }

    heap_caps_free(buf);
    vTaskDelete(NULL);
}

static void __wifi_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (id == VIEW_EVENT_WIFI_ST) {
        struct view_data_wifi_st *p_st = (struct view_data_wifi_st *)event_data;
        bool was_connected = __g_wifi_connected;
        __g_wifi_connected = p_st->is_network;

        if (__g_wifi_connected && !was_connected) {
            ESP_LOGI(TAG, "WiFi connected, triggering DX fetch");
            __fetch_hamqth_spots(NULL);
        }
    }
}

static void __refresh_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (id == VIEW_EVENT_DX_REFRESH_REQ) {
        ESP_LOGI(TAG, "Manual DX refresh requested");
        __fetch_hamqth_spots(NULL);
    }
}

int indicator_dxcluster_init(void)
{
    __g_data_mutex = xSemaphoreCreateMutex();
    memset(&__g_dx_list, 0, sizeof(__g_dx_list));

    /* Periodic HTTP fetch timer */
    const esp_timer_create_args_t timer_args = {
        .callback = __fetch_hamqth_spots,
        .arg = NULL,
        .name = "dx_fetch"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &__g_fetch_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(__g_fetch_timer, DX_FETCH_INTERVAL));

    /* Telnet task */
    xTaskCreate(__telnet_task, "telnet_dx", TELNET_TASK_STACK, NULL, 3, &__g_telnet_task);

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle,
                    VIEW_EVENT_BASE, VIEW_EVENT_WIFI_ST,
                    __wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle,
                    VIEW_EVENT_BASE, VIEW_EVENT_DX_REFRESH_REQ,
                    __refresh_event_handler, NULL, NULL));

    ESP_LOGI(TAG, "DX cluster module initialized");
    return 0;
}

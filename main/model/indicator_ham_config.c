#include "indicator_ham_config.h"
#include "indicator_storage.h"
#include "freertos/semphr.h"
#include <string.h>

#define HAM_CFG_STORAGE    "ham-cfg"
#define ALERT_CFG_STORAGE  "alert-cfg"

static const char *TAG = "ham_config";

static struct view_data_ham_config __g_ham_config;
static struct view_data_alert_config __g_alert_config;
static SemaphoreHandle_t __g_data_mutex;

static void __ham_cfg_set(struct view_data_ham_config *p_cfg)
{
    xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
    memcpy(&__g_ham_config, p_cfg, sizeof(struct view_data_ham_config));
    xSemaphoreGive(__g_data_mutex);
}

void indicator_ham_config_get(struct view_data_ham_config *p_cfg)
{
    xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
    memcpy(p_cfg, &__g_ham_config, sizeof(struct view_data_ham_config));
    xSemaphoreGive(__g_data_mutex);
}

static void __alert_cfg_set(struct view_data_alert_config *p_cfg)
{
    xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
    memcpy(&__g_alert_config, p_cfg, sizeof(struct view_data_alert_config));
    xSemaphoreGive(__g_data_mutex);
}

void indicator_alert_config_get(struct view_data_alert_config *p_cfg)
{
    xSemaphoreTake(__g_data_mutex, portMAX_DELAY);
    memcpy(p_cfg, &__g_alert_config, sizeof(struct view_data_alert_config));
    xSemaphoreGive(__g_data_mutex);
}

static void __ham_cfg_save(struct view_data_ham_config *p_cfg)
{
    esp_err_t ret = indicator_storage_write(HAM_CFG_STORAGE, (void *)p_cfg, sizeof(struct view_data_ham_config));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ham cfg write err:%d", ret);
    } else {
        ESP_LOGI(TAG, "ham cfg write ok");
    }
}

static void __alert_cfg_save(struct view_data_alert_config *p_cfg)
{
    esp_err_t ret = indicator_storage_write(ALERT_CFG_STORAGE, (void *)p_cfg, sizeof(struct view_data_alert_config));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "alert cfg write err:%d", ret);
    } else {
        ESP_LOGI(TAG, "alert cfg write ok");
    }
}

static void __ham_cfg_restore(void)
{
    struct view_data_ham_config cfg;
    size_t len = sizeof(cfg);

    esp_err_t ret = indicator_storage_read(HAM_CFG_STORAGE, (void *)&cfg, &len);
    if (ret == ESP_OK && len == sizeof(cfg)) {
        ESP_LOGI(TAG, "ham cfg restored");
        __ham_cfg_set(&cfg);
    } else {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "ham cfg not found, using defaults");
        } else {
            ESP_LOGE(TAG, "ham cfg read err:%d", ret);
        }
        memset(&cfg, 0, sizeof(cfg));
        strncpy(cfg.callsign, "N0CALL", MAX_CALLSIGN_LEN - 1);
        strncpy(cfg.grid, "AA00aa", MAX_GRID_LEN - 1);
        cfg.dx_source = 0; /* HamQTH HTTP */
        strncpy(cfg.dx_telnet_host, "dxc.nc7j.com", MAX_DX_HOST_LEN - 1);
        cfg.dx_telnet_port = 7373;
        __ham_cfg_set(&cfg);
    }
}

static void __alert_cfg_restore(void)
{
    struct view_data_alert_config cfg;
    size_t len = sizeof(cfg);

    esp_err_t ret = indicator_storage_read(ALERT_CFG_STORAGE, (void *)&cfg, &len);
    if (ret == ESP_OK && len == sizeof(cfg)) {
        ESP_LOGI(TAG, "alert cfg restored");
        __alert_cfg_set(&cfg);
    } else {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "alert cfg not found, using defaults");
        } else {
            ESP_LOGE(TAG, "alert cfg read err:%d", ret);
        }
        memset(&cfg, 0, sizeof(cfg));
        cfg.enabled = false;
        cfg.bands_mask = 0xFF; /* all bands */
        cfg.callsign_pattern[0] = '\0';
        __alert_cfg_set(&cfg);
    }
}

static void __view_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    switch (id)
    {
        case VIEW_EVENT_HAM_CONFIG_APPLY: {
            struct view_data_ham_config *p_cfg = (struct view_data_ham_config *)event_data;
            ESP_LOGI(TAG, "event: HAM_CONFIG_APPLY, call=%s grid=%s", p_cfg->callsign, p_cfg->grid);
            __ham_cfg_set(p_cfg);
            __ham_cfg_save(p_cfg);
            esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_HAM_CONFIG_UPDATE,
                              (void *)p_cfg, sizeof(struct view_data_ham_config), portMAX_DELAY);
            break;
        }
        case VIEW_EVENT_ALERT_CONFIG_APPLY: {
            struct view_data_alert_config *p_cfg = (struct view_data_alert_config *)event_data;
            ESP_LOGI(TAG, "event: ALERT_CONFIG_APPLY, en=%d mask=0x%02x pat=%s",
                     p_cfg->enabled, p_cfg->bands_mask, p_cfg->callsign_pattern);
            __alert_cfg_set(p_cfg);
            __alert_cfg_save(p_cfg);
            esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_ALERT_CONFIG_UPDATE,
                              (void *)p_cfg, sizeof(struct view_data_alert_config), portMAX_DELAY);
            break;
        }
        default:
            break;
    }
}

int indicator_ham_config_init(void)
{
    __g_data_mutex = xSemaphoreCreateMutex();

    __ham_cfg_restore();
    __alert_cfg_restore();

    /* Notify view of initial config */
    struct view_data_ham_config ham_cfg;
    indicator_ham_config_get(&ham_cfg);
    esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_HAM_CONFIG_UPDATE,
                      (void *)&ham_cfg, sizeof(ham_cfg), portMAX_DELAY);

    struct view_data_alert_config alert_cfg;
    indicator_alert_config_get(&alert_cfg);
    esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_ALERT_CONFIG_UPDATE,
                      (void *)&alert_cfg, sizeof(alert_cfg), portMAX_DELAY);

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle,
                    VIEW_EVENT_BASE, VIEW_EVENT_HAM_CONFIG_APPLY,
                    __view_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle,
                    VIEW_EVENT_BASE, VIEW_EVENT_ALERT_CONFIG_APPLY,
                    __view_event_handler, NULL, NULL));

    return 0;
}

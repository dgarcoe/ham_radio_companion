#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "lv_port.h"
#include "esp_event.h"
#include "esp_event_base.h"

#include "indicator_model.h"
#include "indicator_view.h"
#include "indicator_controller.h"

static const char *TAG = "app_main";

#define VERSION   "v2.0.0"

#define HAM_BANNER  "\n\
  _   _                   ____           _ _         \n\
 | | | | __ _ _ __ ___   |  _ \\ __ _  __| (_) ___   \n\
 | |_| |/ _` | '_ ` _ \\  | |_) / _` |/ _` | |/ _ \\ \n\
 |  _  | (_| | | | | | | |  _ < (_| | (_| | | (_) | \n\
 |_| |_|\\__,_|_| |_| |_| |_| \\_\\__,_|\\__,_|_|\\___/ \n\
   ____                                  _             \n\
  / ___|___  _ __ ___  _ __   __ _ _ __ (_) ___  _ __  \n\
 | |   / _ \\| '_ ` _ \\| '_ \\ / _` | '_ \\| |/ _ \\| '_ \\ \n\
 | |__| (_) | | | | | | |_) | (_| | | | | | (_) | | | |\n\
  \\____\\___/|_| |_| |_| .__/ \\__,_|_| |_|_|\\___/|_| |_|\n\
                       |_|                              \n\
--------------------------------------------------------\n\
 Version: %s %s %s\n\
--------------------------------------------------------\n\
"

ESP_EVENT_DEFINE_BASE(VIEW_EVENT_BASE);
esp_event_loop_handle_t view_event_handle;


void app_main(void)
{
    ESP_LOGI("", HAM_BANNER, VERSION, __DATE__, __TIME__);

    ESP_ERROR_CHECK(bsp_board_init());
    lv_port_init();


    esp_event_loop_args_t view_event_task_args = {
        .queue_size = 20,
        .task_name = "view_event_task",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 12288,
        .task_core_id = tskNO_AFFINITY
    };

    ESP_ERROR_CHECK(esp_event_loop_create(&view_event_task_args, &view_event_handle));


    lv_port_sem_take();
    indicator_view_init();
    lv_port_sem_give();

    indicator_model_init();
    indicator_controller_init();

    static char buffer[128];    /* Make sure buffer is enough for `sprintf` */
    while (1) {
        // sprintf(buffer, "   Biggest /     Free /    Total\n"
        //         "\t  DRAM : [%8d / %8d / %8d]\n"
        //         "\t PSRAM : [%8d / %8d / %8d]",
        //         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        //         heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        //         heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
        //         heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
        //         heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        //         heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        // ESP_LOGI("MEM", "%s", buffer);

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

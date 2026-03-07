#include "indicator_model.h"
#include "indicator_storage.h"
#include "indicator_wifi.h"
#include "indicator_display.h"
#include "indicator_time.h"
#include "indicator_btn.h"
#include "indicator_ham_config.h"
#include "indicator_propagation.h"
#include "indicator_dxcluster.h"
#include "indicator_alert.h"

int indicator_model_init(void)
{
    indicator_storage_init();
    indicator_wifi_init();
    indicator_time_init();
    indicator_ham_config_init();
    indicator_propagation_init();
    indicator_dxcluster_init();
    indicator_alert_init();
    indicator_display_init();  // lcd bl on
    indicator_btn_init();
}

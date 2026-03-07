#ifndef INDICATOR_HAM_CONFIG_H
#define INDICATOR_HAM_CONFIG_H

#include "config.h"
#include "view_data.h"

#ifdef __cplusplus
extern "C" {
#endif

int indicator_ham_config_init(void);
void indicator_ham_config_get(struct view_data_ham_config *p_cfg);
void indicator_alert_config_get(struct view_data_alert_config *p_cfg);

#ifdef __cplusplus
}
#endif

#endif

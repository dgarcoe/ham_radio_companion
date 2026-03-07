#ifndef INDICATOR_ALERT_H
#define INDICATOR_ALERT_H

#include "config.h"
#include "view_data.h"

#ifdef __cplusplus
extern "C" {
#endif

int indicator_alert_init(void);
bool indicator_alert_check_spot(struct view_data_dx_spot *spot);

#ifdef __cplusplus
}
#endif

#endif

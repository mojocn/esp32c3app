#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "cJSON.h"
#include "config.h"

void wifi_init();

void wifi_config_apply(const AppConfig *config);

cJSON *wifi_status();

#endif // WIFI_MANAGER_H

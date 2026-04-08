#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "config.h"
#include "esp_err.h"


void mqtt_manager_start(void);
void mqtt_manager_stop(void);



esp_err_t mqtt_manager_publish(const char *topic, const char *data);

#endif // MQTT_MANAGER_H

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "config.h"
#include "esp_err.h"

typedef struct {
    bool connected;
    char host[128];
    uint16_t port;
    char username[64];
} MqttStatus;

void mqtt_manager_start(void);
void mqtt_manager_stop(void);

MqttStatus mqtt_manager_get_status(void);

esp_err_t mqtt_manager_publish(const char *topic, const char *data);

#endif // MQTT_MANAGER_H

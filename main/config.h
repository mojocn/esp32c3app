#ifndef CONFIG_H
#define CONFIG_H

#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>

/* MQTT Configuration */
#define MQTT_HEARTBEAT_INTERVAL_S 60 /* Heartbeat every 10 seconds */

char *device_name();

typedef struct {
  char *device_name;

  char *wifi_ap_ssid;
  char *wifi_ap_password;
  bool wifi_ap_enabled;

  char *wifi_sta_ssid;
  char *wifi_sta_password;
  bool wifi_sta_enabled;

  char *mqtt_broker_host;
  uint16_t mqtt_broker_port;
  char *mqtt_username;
  char *mqtt_password;

} AppConfig;

AppConfig *config_from_json(cJSON *json_str);
cJSON *config_to_json(const AppConfig *config);

AppConfig *config_init();
AppConfig *config_get();
void config_free(AppConfig *config);
void config_save(const AppConfig *config);

#endif // CONFIG_H

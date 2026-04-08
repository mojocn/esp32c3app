#include "config.h"

#include "esp_log.h"
#include "esp_mac.h"
#include <nvs_flash.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *product_name = "Shelly.EricZ";
static const char *KEY_CONFIG = "config";
static const char *KEY_NAMESPACE = "app";

static void kv_str_set(const char *key, const char *value) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(KEY_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE("CONFIG", "kv_str_set(): nvs_open failed (%s)", esp_err_to_name(err));
    return; // Failed to open NVS
  }

  err = nvs_set_str(handle, key, value);
  if (err != ESP_OK) {
    ESP_LOGE("CONFIG", "kv_str_set(): nvs_set_str failed (%s)", esp_err_to_name(err));
  } else {
    err = nvs_commit(handle);
    if (err != ESP_OK) {
      ESP_LOGE("CONFIG", "kv_str_set(): nvs_commit failed (%s)", esp_err_to_name(err));
    }
  }

  nvs_close(handle);
}

static char *kv_str_get(const char *key) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(KEY_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    return NULL; // Failed to open NVS
  }
  size_t required_size = 0;
  err = nvs_get_str(handle, key, NULL, &required_size);
  if (err != ESP_OK || required_size == 0) {
    nvs_close(handle);
    return NULL; // Key not found or empty
  }
  char *value = malloc(required_size);
  if (!value) {
    nvs_close(handle);
    return NULL; // Memory allocation failed
  }
  err = nvs_get_str(handle, key, value, &required_size);
  nvs_close(handle);
  if (err != ESP_OK) {
    free(value);
    return NULL; // Failed to read value
  }
  return value;
}

char *device_name() {
  static char name_with_mac[32];
  if (name_with_mac[0] != '\0') {
    return name_with_mac;
  }

  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  snprintf(name_with_mac, sizeof(name_with_mac), "%s-%02x%02x%02x", product_name, mac[3], mac[4], mac[5]);
  return name_with_mac;
}

static char *strdup_safe(const char *s) {
  if (!s) {
    return NULL;
  }
  size_t len = strlen(s);
  char *copy = malloc(len + 1);
  if (!copy) {
    return NULL;
  }
  memcpy(copy, s, len + 1);
  return copy;
}

static AppConfig *app_config_default() {
  AppConfig *cfg = calloc(1, sizeof(AppConfig));
  if (!cfg) {
    return NULL;
  }
  char *this_device_name = device_name();
  cfg->device_name = strdup_safe(this_device_name);
  cfg->wifi_ap_ssid = strdup_safe(this_device_name);
  cfg->wifi_ap_password = NULL;
  cfg->wifi_ap_enabled = true;

  cfg->wifi_sta_ssid = strdup_safe("");
  cfg->wifi_sta_password = strdup_safe("");
  cfg->wifi_sta_enabled = true;

  cfg->mqtt_broker_host = strdup_safe("");
  cfg->mqtt_broker_port = 1883;
  cfg->mqtt_username = strdup_safe("");
  cfg->mqtt_password = strdup_safe("");

  return cfg;
}

AppConfig *config_from_json(cJSON *json) {
  if (!json || !cJSON_IsObject(json)) {
    return NULL;
  }

  AppConfig *cfg = calloc(1, sizeof(AppConfig));
  if (!cfg) {
    return NULL;
  }

  cJSON *item = cJSON_GetObjectItemCaseSensitive(json, "device_name");
  if (cJSON_IsString(item) && item->valuestring) {
    cfg->device_name = strdup_safe(item->valuestring);
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "wifi_ap_ssid");
  if (cJSON_IsString(item) && item->valuestring) {
    cfg->wifi_ap_ssid = strdup_safe(item->valuestring);
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "wifi_ap_password");
  if (cJSON_IsString(item) && item->valuestring) {
    cfg->wifi_ap_password = strdup_safe(item->valuestring);
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "wifi_ap_enabled");
  if (cJSON_IsBool(item)) {
    cfg->wifi_ap_enabled = cJSON_IsTrue(item);
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "wifi_sta_ssid");
  if (cJSON_IsString(item) && item->valuestring) {
    cfg->wifi_sta_ssid = strdup_safe(item->valuestring);
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "wifi_sta_password");
  if (cJSON_IsString(item) && item->valuestring) {
    cfg->wifi_sta_password = strdup_safe(item->valuestring);
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "wifi_sta_enabled");
  if (cJSON_IsBool(item)) {
    cfg->wifi_sta_enabled = cJSON_IsTrue(item);
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "mqtt_broker_host");
  if (cJSON_IsString(item) && item->valuestring) {
    cfg->mqtt_broker_host = strdup_safe(item->valuestring);
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "mqtt_broker_port");
  if (cJSON_IsNumber(item)) {
    cfg->mqtt_broker_port = (uint16_t)item->valueint;
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "mqtt_username");
  if (cJSON_IsString(item) && item->valuestring) {
    cfg->mqtt_username = strdup_safe(item->valuestring);
  }

  item = cJSON_GetObjectItemCaseSensitive(json, "mqtt_password");
  if (cJSON_IsString(item) && item->valuestring) {
    cfg->mqtt_password = strdup_safe(item->valuestring);
  }

  return cfg;
}

cJSON *config_to_json(const AppConfig *config) {
  if (!config) {
    return NULL;
  }

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    return NULL;
  }

  cJSON_AddStringToObject(json, "device_name", config->device_name ? config->device_name : "");
  cJSON_AddStringToObject(json, "wifi_ap_ssid", config->wifi_ap_ssid ? config->wifi_ap_ssid : "");
  cJSON_AddStringToObject(json, "wifi_ap_password", config->wifi_ap_password ? config->wifi_ap_password : "");
  cJSON_AddBoolToObject(json, "wifi_ap_enabled", config->wifi_ap_enabled);
  cJSON_AddStringToObject(json, "wifi_sta_ssid", config->wifi_sta_ssid ? config->wifi_sta_ssid : "");
  cJSON_AddStringToObject(json, "wifi_sta_password", config->wifi_sta_password ? config->wifi_sta_password : "");
  cJSON_AddBoolToObject(json, "wifi_sta_enabled", config->wifi_sta_enabled);
  cJSON_AddStringToObject(json, "mqtt_broker_host", config->mqtt_broker_host ? config->mqtt_broker_host : "");
  cJSON_AddNumberToObject(json, "mqtt_broker_port", config->mqtt_broker_port);
  cJSON_AddStringToObject(json, "mqtt_username", config->mqtt_username ? config->mqtt_username : "");
  cJSON_AddStringToObject(json, "mqtt_password", config->mqtt_password ? config->mqtt_password : "");

  return json;
}

void config_free(AppConfig *config) {
  if (!config) {
    return;
  }

  free(config->device_name);
  free(config->wifi_ap_ssid);
  free(config->wifi_ap_password);
  free(config->wifi_sta_ssid);
  free(config->wifi_sta_password);
  free(config->mqtt_broker_host);
  free(config->mqtt_username);
  free(config->mqtt_password);

  free(config);
}

AppConfig *config_get() {
  char *json_str = kv_str_get(KEY_CONFIG);
  if (!json_str) {
    return NULL;
  }
  cJSON *json = cJSON_Parse(json_str);
  if (!json) {
    free(json_str);
    return NULL;
  }
  AppConfig *cfg = config_from_json(json);
  cJSON_Delete(json);
  free(json_str);
  return cfg;
}

void config_save(const AppConfig *config) {
  if (!config) {
    return;
  }

  cJSON *json = config_to_json(config);
  char *json_str = cJSON_PrintUnformatted(json);
  cJSON_Delete(json);
  kv_str_set(KEY_CONFIG, json_str);
  free(json_str);
}

AppConfig *config_init() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      ESP_LOGE("CONFIG", "nvs_flash_erase failed (%s)", esp_err_to_name(err));
      return NULL;
    }
    err = nvs_flash_init();
  }

  if (err != ESP_OK) {
    ESP_LOGE("CONFIG", "nvs_flash_init failed (%s)", esp_err_to_name(err));
    return NULL;
  }

  AppConfig *cfg = config_get();
  if (!cfg) {
    cfg = app_config_default();
    config_save(cfg);
    return cfg;
  }

  /* Back-fill any MQTT fields missing from an older saved config */
  AppConfig *defaults = app_config_default();
  if (defaults) {
    if (!cfg->mqtt_broker_host || cfg->mqtt_broker_host[0] == '\0') {
      free(cfg->mqtt_broker_host);
      cfg->mqtt_broker_host = defaults->mqtt_broker_host;
      defaults->mqtt_broker_host = NULL;
    }
    if (cfg->mqtt_broker_port == 0) {
      cfg->mqtt_broker_port = defaults->mqtt_broker_port;
    }
    if (!cfg->mqtt_username || cfg->mqtt_username[0] == '\0') {
      free(cfg->mqtt_username);
      cfg->mqtt_username = defaults->mqtt_username;
      defaults->mqtt_username = NULL;
    }
    if (!cfg->mqtt_password || cfg->mqtt_password[0] == '\0') {
      free(cfg->mqtt_password);
      cfg->mqtt_password = defaults->mqtt_password;
      defaults->mqtt_password = NULL;
    }
    config_free(defaults);
    config_save(cfg); /* persist the filled-in fields */
  }

  return cfg;
}

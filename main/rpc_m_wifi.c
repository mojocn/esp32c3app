#include "rpc_m_wifi.h"
#include "config.h"
#include "wifi_manager.h"

#include "rpc_m.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static esp_err_t replace_config_string(char **dst, const char *value) {
  char *new_value = NULL;
  if (value) {
    new_value = strdup(value);
    if (!new_value) {
      return ESP_ERR_NO_MEM;
    }
  }

  free(*dst);
  *dst = new_value;
  return ESP_OK;
}

static AppConfig *load_config_or_default(void) {
  AppConfig *config = config_get();
  if (!config) {
    config = config_init();
  }
  return config;
}

JsonRpcResponse *m_wifi_info(cJSON *params) {
  (void)params;
  cJSON *info = wifi_status();
  if (!info) {
    return jsonrpc_response_create(NULL, "Failed to read WiFi status", JSONRPC_INTERNAL_ERROR);
  }
  return jsonrpc_response_create(info, NULL, 0);
}

JsonRpcResponse *m_wifi_sta_set(cJSON *params) {
  if (!cJSON_IsObject(params)) {
    return jsonrpc_response_create(NULL, "Invalid params: expected an object with 'enable' and optional 'ssid','password'", JSONRPC_INVALID_PARAMS);
  }

  cJSON *ssid_json = cJSON_GetObjectItem(params, "ssid");
  cJSON *password_json = cJSON_GetObjectItem(params, "password");
  cJSON *enable_json = cJSON_GetObjectItem(params, "enable");

  if (!cJSON_IsBool(enable_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'enable' is required and must be a boolean", JSONRPC_INVALID_PARAMS);
  }

  if (ssid_json && !cJSON_IsString(ssid_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'ssid' must be a string", JSONRPC_INVALID_PARAMS);
  }
  if (password_json && !cJSON_IsString(password_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'password' must be a string", JSONRPC_INVALID_PARAMS);
  }

  const bool enable = cJSON_IsTrue(enable_json);
  const char *ssid = (ssid_json && cJSON_IsString(ssid_json)) ? ssid_json->valuestring : NULL;
  const char *password = (password_json && cJSON_IsString(password_json)) ? password_json->valuestring : NULL;

  AppConfig *config = load_config_or_default();
  if (!config) {
    return jsonrpc_response_create(NULL, "Failed to load config", JSONRPC_INTERNAL_ERROR);
  }

  esp_err_t err = ESP_OK;
  config->wifi_sta_enabled = enable;

  if (ssid) {
    err = replace_config_string(&config->wifi_sta_ssid, ssid);
  }
  if (err == ESP_OK && password_json) {
    err = replace_config_string(&config->wifi_sta_password, password ? password : "");
  }

  if (err != ESP_OK) {
    config_free(config);
    return jsonrpc_response_create(NULL, "Out of memory while updating STA config", JSONRPC_INTERNAL_ERROR);
  }

  if (enable && (!config->wifi_sta_ssid || config->wifi_sta_ssid[0] == '\0')) {
    config_free(config);
    return jsonrpc_response_create(NULL, "STA SSID is empty", JSONRPC_INVALID_PARAMS);
  }

  config_save(config);
  wifi_config_apply(config);

  cJSON *result = cJSON_CreateObject();
  if (!result) {
    config_free(config);
    return jsonrpc_response_create(NULL, "Out of memory while building response", JSONRPC_INTERNAL_ERROR);
  }
  if (config->wifi_sta_ssid && config->wifi_sta_ssid[0] != '\0') {
    cJSON_AddStringToObject(result, "ssid", config->wifi_sta_ssid);
  }
  cJSON_AddBoolToObject(result, "enabled", enable);
  cJSON_AddStringToObject(result, "status", enable ? "connecting" : "disabled");
  config_free(config);
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_wifi_ap_set(cJSON *params) {
  if (!cJSON_IsObject(params)) {
    return jsonrpc_response_create(NULL, "Invalid params: expected an object with 'enable' and optional 'password','ssid'", JSONRPC_INVALID_PARAMS);
  }
  cJSON *enable_json = cJSON_GetObjectItem(params, "enable");
  cJSON *ssid_json = cJSON_GetObjectItem(params, "ssid");
  cJSON *password_json = cJSON_GetObjectItem(params, "password");

  if (!cJSON_IsBool(enable_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'enable' is required and must be a boolean", JSONRPC_INVALID_PARAMS);
  }
  if (ssid_json && !cJSON_IsString(ssid_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'ssid' must be a string", JSONRPC_INVALID_PARAMS);
  }
  if (password_json && !cJSON_IsString(password_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'password' must be a string", JSONRPC_INVALID_PARAMS);
  }

  bool enable = cJSON_IsTrue(enable_json);
  const char *ssid = (ssid_json && cJSON_IsString(ssid_json)) ? ssid_json->valuestring : NULL;
  const char *password = (password_json && cJSON_IsString(password_json)) ? password_json->valuestring : NULL;

  AppConfig *config = load_config_or_default();
  if (!config) {
    return jsonrpc_response_create(NULL, "Failed to load config", JSONRPC_INTERNAL_ERROR);
  }

  esp_err_t err = ESP_OK;
  config->wifi_ap_enabled = enable;

  if (ssid) {
    err = replace_config_string(&config->wifi_ap_ssid, ssid);
  }
  if (err == ESP_OK && password_json) {
    err = replace_config_string(&config->wifi_ap_password, password ? password : "");
  }

  if (err != ESP_OK) {
    config_free(config);
    return jsonrpc_response_create(NULL, "Out of memory while updating AP config", JSONRPC_INTERNAL_ERROR);
  }

  if (enable && (!config->wifi_ap_ssid || config->wifi_ap_ssid[0] == '\0')) {
    err = replace_config_string(&config->wifi_ap_ssid, device_name());
    if (err != ESP_OK) {
      config_free(config);
      return jsonrpc_response_create(NULL, "Out of memory while setting default AP SSID", JSONRPC_INTERNAL_ERROR);
    }
  }

  config_save(config);
  wifi_config_apply(config);

  cJSON *result = cJSON_CreateObject();
  if (!result) {
    config_free(config);
    return jsonrpc_response_create(NULL, "Out of memory while building response", JSONRPC_INTERNAL_ERROR);
  }
  if (config->wifi_ap_ssid && config->wifi_ap_ssid[0] != '\0') {
    cJSON_AddStringToObject(result, "ssid", config->wifi_ap_ssid);
  }
  cJSON_AddBoolToObject(result, "enabled", enable);
  cJSON_AddStringToObject(result, "status", enable ? "enabled" : "disabled");
  config_free(config);
  return jsonrpc_response_create(result, NULL, 0);
}

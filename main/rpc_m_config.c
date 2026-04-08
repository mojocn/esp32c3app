#include "rpc_m_config.h"
#include "config.h"

#include "cJSON.h"
#include "rpc_json.h"

#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static char *duplicate_string(const char *value) {
  if (!value) {
    return NULL;
  }
  size_t len = strlen(value);
  char *out = malloc(len + 1);
  if (!out) {
    return NULL;
  }
  memcpy(out, value, len + 1);
  return out;
}

static void apply_string_field(char **dst, const char *value) {
  if (value) {
    free(*dst);
    *dst = duplicate_string(value);
  }
}

JsonRpcResponse *m_config_get(cJSON *params) {
  (void)params;

  AppConfig *cfg = config_get();
  if (!cfg) {
    cfg = config_init();
    if (!cfg) {
      return jsonrpc_response_create(NULL, "Failed to load config", JSONRPC_INTERNAL_ERROR);
    }
  }

  cJSON *result = config_to_json(cfg);
  config_free(cfg);
  if (!result) {
    return jsonrpc_response_create(NULL, NULL, JSONRPC_INTERNAL_ERROR);
  }
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_config_set(cJSON *params) {
  if (!cJSON_IsObject(params)) {
    return jsonrpc_response_create(NULL, "Invalid params: expected object", JSONRPC_INVALID_PARAMS);
  }

  AppConfig *cfg = config_get();
  if (!cfg) {
    cfg = config_init();
    if (!cfg) {
      return jsonrpc_response_create(NULL, "Failed to load config", JSONRPC_INTERNAL_ERROR);
    }
  }

  cJSON *item;

  item = cJSON_GetObjectItem(params, "device_name");
  if (cJSON_IsString(item) && item->valuestring) {
    apply_string_field(&cfg->device_name, item->valuestring);
  }

  item = cJSON_GetObjectItem(params, "wifi_ap_ssid");
  if (cJSON_IsString(item) && item->valuestring) {
    apply_string_field(&cfg->wifi_ap_ssid, item->valuestring);
  }

  item = cJSON_GetObjectItem(params, "wifi_ap_password");
  if (cJSON_IsString(item) && item->valuestring) {
    apply_string_field(&cfg->wifi_ap_password, item->valuestring);
  }

  item = cJSON_GetObjectItem(params, "wifi_ap_enabled");
  if (cJSON_IsBool(item)) {
    cfg->wifi_ap_enabled = cJSON_IsTrue(item);
  }

  item = cJSON_GetObjectItem(params, "wifi_sta_ssid");
  if (cJSON_IsString(item) && item->valuestring) {
    apply_string_field(&cfg->wifi_sta_ssid, item->valuestring);
  }

  item = cJSON_GetObjectItem(params, "wifi_sta_password");
  if (cJSON_IsString(item) && item->valuestring) {
    apply_string_field(&cfg->wifi_sta_password, item->valuestring);
  }

  item = cJSON_GetObjectItem(params, "wifi_sta_enabled");
  if (cJSON_IsBool(item)) {
    cfg->wifi_sta_enabled = cJSON_IsTrue(item);
  }

  config_save(cfg);

  cJSON *response_config = config_to_json(cfg);
  config_free(cfg);

  if (!response_config) {
    return jsonrpc_response_create(NULL, "Failed to build response", JSONRPC_INTERNAL_ERROR);
  }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "status", "ok");
  cJSON_AddItemToObject(result, "config", response_config);

  return jsonrpc_response_create(result, NULL, 0);
}

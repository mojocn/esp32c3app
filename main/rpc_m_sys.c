#include "rpc_m_sys.h"

#include "cJSON.h"
#include "config.h"
#include "esp_chip_info.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "ota_manager.h"
#include "sdkconfig.h"

static void sys_restart_timer_callback(void *arg) {
  (void)arg;
  esp_restart();
}

JsonRpcResponse *m_sys_info(cJSON *params) {
  cJSON *info = cJSON_CreateObject();

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  cJSON_AddStringToObject(info, "model", CONFIG_IDF_TARGET);
  cJSON_AddNumberToObject(info, "cores", chip_info.cores);
  cJSON_AddNumberToObject(info, "revision", chip_info.revision);
  cJSON_AddNumberToObject(info, "free_heap", esp_get_free_heap_size());
  cJSON_AddStringToObject(info, "idf_version", esp_get_idf_version());
  cJSON_AddStringToObject(info, "device_name", device_name());

  return jsonrpc_response_create(info, NULL, 0);
}

JsonRpcResponse *m_sys_ota(cJSON *params) {
  if (!cJSON_IsObject(params)) {
    return jsonrpc_response_create(NULL, "Invalid params: expected an object with 'url'", JSONRPC_INVALID_PARAMS);
  }

  cJSON *url_json = cJSON_GetObjectItem(params, "url");
  if (!cJSON_IsString(url_json) || url_json->valuestring == NULL) {
    return jsonrpc_response_create(NULL, "Invalid params: 'url' is required", JSONRPC_INVALID_PARAMS);
  }

  esp_err_t err = ota_manager_start(url_json->valuestring);
  cJSON *result = cJSON_CreateObject();
  if (err == ESP_OK) {
    cJSON_AddStringToObject(result, "status", "updating");
    cJSON_AddStringToObject(result, "url", url_json->valuestring);
  } else {
    cJSON_AddStringToObject(result, "status", "error");
    cJSON_AddStringToObject(result, "reason", esp_err_to_name(err));
  }
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_sys_reboot(cJSON *params) {
  (void)params;

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "status", "rebooting");

  esp_timer_create_args_t timer_args = {
      .callback = sys_restart_timer_callback,
      .arg = NULL,
      .name = "sys_reboot_timer",
      .dispatch_method = ESP_TIMER_TASK,
  };

  esp_timer_handle_t timer;
  esp_err_t err = esp_timer_create(&timer_args, &timer);
  if (err != ESP_OK) {
    cJSON_AddStringToObject(result, "warning", "failed to schedule reboot");
    return jsonrpc_response_create(result, NULL, 0);
  }

  err = esp_timer_start_once(timer, 500000); // 500 ms delay to allow response round trip
  if (err != ESP_OK) {
    cJSON_AddStringToObject(result, "warning", "failed to start reboot timer");
    esp_timer_delete(timer);
    return jsonrpc_response_create(result, NULL, 0);
  }

  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_sys_factory(cJSON *params) {
  (void)params;

  esp_err_t err = nvs_flash_erase();
  if (err != ESP_OK) {
    return jsonrpc_response_create(NULL, "Failed to erase NVS", JSONRPC_INTERNAL_ERROR);
  }

  err = nvs_flash_init();
  if (err != ESP_OK) {
    return jsonrpc_response_create(NULL, "Failed to reinitialize NVS after erase", JSONRPC_INTERNAL_ERROR);
  }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "status", "factory_reset");
  cJSON_AddStringToObject(result, "next", "rebooting");

  esp_timer_create_args_t timer_args = {
      .callback = sys_restart_timer_callback,
      .arg = NULL,
      .name = "sys_factory_timer",
      .dispatch_method = ESP_TIMER_TASK,
  };

  esp_timer_handle_t timer;
  err = esp_timer_create(&timer_args, &timer);
  if (err != ESP_OK) {
    cJSON_AddStringToObject(result, "warning", "failed to schedule reboot after factory reset");
    return jsonrpc_response_create(result, NULL, 0);
  }

  err = esp_timer_start_once(timer, 500000); // 500 ms delay to allow response round trip
  if (err != ESP_OK) {
    cJSON_AddStringToObject(result, "warning", "failed to start reboot timer");
    esp_timer_delete(timer);
  }

  return jsonrpc_response_create(result, NULL, 0);
}

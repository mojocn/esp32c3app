#include "rpc_m_ir.h"

#include "m5stack_unit_ir.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

#define IR_NVS_NAMESPACE "ir_codes"

JsonRpcResponse *m_ir_send(cJSON *params) {
  cJSON *addr_json = cJSON_GetObjectItem(params, "addr");
  cJSON *cmd_json = cJSON_GetObjectItem(params, "cmd");
  if (!cJSON_IsNumber(addr_json) || !cJSON_IsNumber(cmd_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'addr' and 'cmd' must be numbers", JSONRPC_INVALID_PARAMS);
  }

  int addr = addr_json->valueint;
  int cmd = cmd_json->valueint;
  if (addr < 0 || addr > 255 || cmd < 0 || cmd > 255) {
    return jsonrpc_response_create(NULL, "Invalid params: 'addr' and 'cmd' must be in the range 0-255", JSONRPC_INVALID_PARAMS);
  }

  ir_send_nec((uint8_t)addr, (uint8_t)cmd);

  cJSON *result = cJSON_CreateObject();
  cJSON_AddNumberToObject(result, "addr", addr);
  cJSON_AddNumberToObject(result, "cmd", cmd);
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_ir_record(cJSON *params) {
  cJSON *name_json = cJSON_GetObjectItem(params, "name");
  if (!cJSON_IsString(name_json) || name_json->valuestring[0] == '\0') {
    return jsonrpc_response_create(NULL, "Invalid params: 'name' must be a non-empty string", JSONRPC_INVALID_PARAMS);
  }
  const char *name = name_json->valuestring;
  if (strlen(name) > 15) {
    return jsonrpc_response_create(NULL, "Invalid params: 'name' must be 15 characters or fewer", JSONRPC_INVALID_PARAMS);
  }

  uint32_t timeout_ms = 10000;
  cJSON *timeout_json = cJSON_GetObjectItem(params, "timeout_ms");
  if (cJSON_IsNumber(timeout_json) && timeout_json->valueint > 0) {
    timeout_ms = (uint32_t)timeout_json->valueint;
  }

  uint8_t addr, cmd;
  if (!ir_record_nec(&addr, &cmd, timeout_ms)) {
    return jsonrpc_response_create(NULL, "Timeout: no IR signal received", JSONRPC_INTERNAL_ERROR);
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(IR_NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return jsonrpc_response_create(NULL, "Failed to open NVS", JSONRPC_INTERNAL_ERROR);
  }
  uint8_t data[2] = {addr, cmd};
  err = nvs_set_blob(handle, name, data, sizeof(data));
  if (err == ESP_OK) {
    nvs_commit(handle);
  }
  nvs_close(handle);

  if (err != ESP_OK) {
    return jsonrpc_response_create(NULL, "Failed to save IR code to NVS", JSONRPC_INTERNAL_ERROR);
  }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "name", name);
  cJSON_AddNumberToObject(result, "addr", addr);
  cJSON_AddNumberToObject(result, "cmd", cmd);
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_ir_list(cJSON *params) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(IR_NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return jsonrpc_response_create(cJSON_CreateArray(), NULL, 0);
  }
  if (err != ESP_OK) {
    return jsonrpc_response_create(NULL, "Failed to open NVS", JSONRPC_INTERNAL_ERROR);
  }

  cJSON *list = cJSON_CreateArray();

  nvs_iterator_t it = NULL;
  err = nvs_entry_find_in_handle(handle, NVS_TYPE_BLOB, &it);
  while (err == ESP_OK && it != NULL) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);

    uint8_t data[2];
    size_t len = sizeof(data);
    if (nvs_get_blob(handle, info.key, data, &len) == ESP_OK && len == 2) {
      cJSON *entry = cJSON_CreateObject();
      cJSON_AddStringToObject(entry, "name", info.key);
      cJSON_AddNumberToObject(entry, "addr", data[0]);
      cJSON_AddNumberToObject(entry, "cmd", data[1]);
      cJSON_AddItemToArray(list, entry);
    }

    err = nvs_entry_next(&it);
  }
  if (it) nvs_release_iterator(it);

  nvs_close(handle);
  return jsonrpc_response_create(list, NULL, 0);
}

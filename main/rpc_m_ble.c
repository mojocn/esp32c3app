#include "rpc_m_ble.h"

#include "rpc_m.h"

#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>

#include <stdlib.h>
#include <string.h>

static const char *TAG = "RPC_BLE";

JsonRpcResponse *m_ble_info(cJSON *params) {
  cJSON *info = cJSON_CreateObject();

  uint8_t mac[6];
  esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_get_mac failed: %s", esp_err_to_name(err));
    return jsonrpc_response_create(NULL, "Failed to get MAC address", JSONRPC_INTERNAL_ERROR);
  }

  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  cJSON_AddStringToObject(info, "mac", mac_str);
  cJSON_AddBoolToObject(info, "enabled", true);
  cJSON_AddStringToObject(info, "status", "active");

  return jsonrpc_response_create(info, NULL, 0);
}

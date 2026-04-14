#include "rpc_m.h"

#include "buzzer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "gpio_led.h"
#include "rpc_json.h"
#include "rpc_m_ble.h"
#include "rpc_m_config.h"
#include "rpc_m_cron.h"
#include "rpc_m_display.h"
#include "rpc_m_ht.h"
#include "rpc_m_ir.h"
#include "rpc_m_light.h"
#include "rpc_m_mqtt.h"
#include "rpc_m_sys.h"
#include "rpc_m_wifi.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* RPC Method declarations */
static JsonRpcResponse *m_sys_methods(cJSON *params);

typedef struct {
  const char *method;
  rpc_handler_t handler;
} RpcMethodEntry;

static const RpcMethodEntry rpc_methods[] = {
    {"Sys.Info", m_sys_info},       {"Sys.Reboot", m_sys_reboot},         {"Sys.Factory", m_sys_factory},     {"Sys.Methods", m_sys_methods},     {"Wifi.Sta.Set", m_wifi_sta_set}, {"Wifi.Ap.Set", m_wifi_ap_set}, {"Wifi.Info", m_wifi_info}, {"Ble.Info", m_ble_info}, {"Sys.Ota", m_sys_ota},
    {"Ht.Info", m_ht_info},         {"Display.Effect", m_display_effect}, {"Light.Led.Set", m_light_led_set}, {"Light.Rgb.Set", m_light_rgb_set}, {"Config.Get", m_config_get},     {"Config.Set", m_config_set},   {"Cron.List", m_cron_list}, {"Cron.Get", m_cron_get}, {"Cron.Create", m_cron_create},
    {"Cron.Update", m_cron_update}, {"Cron.Delete", m_cron_delete},       {"Mqtt.Set", m_mqtt_set},           {"Mqtt.Info", m_mqtt_info},         {"Ir.Send", m_ir_send},           {"Ir.Record", m_ir_record},     {"Ir.List", m_ir_list},
};
static int rpc_method_count = sizeof(rpc_methods) / sizeof(rpc_methods[0]);

static rpc_handler_t find_rpc_handler(const char *method) {
  if (!method) {
    return NULL;
  }
  for (int i = 0; i < rpc_method_count; i++) {
    if (strcasecmp(method, rpc_methods[i].method) == 0) {
      return rpc_methods[i].handler;
    }
  }
  return NULL;
}

static JsonRpcResponse *m_sys_methods(cJSON *params) {
  (void)params;
  cJSON *methods = cJSON_CreateArray();
  for (int i = 0; i < rpc_method_count; i++) {
    cJSON_AddItemToArray(methods, cJSON_CreateString(rpc_methods[i].method));
  }
  return jsonrpc_response_create(methods, NULL, 0);
}

static char *rpc_strdup(const char *input) {
  if (!input) {
    return NULL;
  }

  size_t len = strlen(input);
  char *copy = malloc(len + 1);
  if (!copy) {
    return NULL;
  }
  memcpy(copy, input, len + 1);
  return copy;
}

static char *rpc_build_fallback_error(void) { return rpc_strdup("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":null}"); }

char *rpc_process_request(const char *request_str) {

  gpio_led_blink();
  JsonRpcRequest *request = jsonrpc_parse_request(request_str);
  JsonRpcResponse *response = NULL;
  if (!request) {
    response = jsonrpc_response_create(NULL, "Parse error: invalid JSON", JSONRPC_PARSE_ERROR);
  } else if (strcmp(request->jsonrpc, "2.0") != 0) {
    response = jsonrpc_response_create(NULL, "Invalid Request: jsonrpc version must be '2.0'", JSONRPC_INVALID_REQUEST);
  } else if (!request->method) {
    response = jsonrpc_response_create(NULL, "Invalid Request: 'method' is required", JSONRPC_INVALID_REQUEST);
  } else {
    rpc_handler_t handler = find_rpc_handler(request->method);
    if (handler) {
      response = handler(request->params);
    } else {
      response = jsonrpc_response_create(NULL, "Method not found", JSONRPC_METHOD_NOT_FOUND);
    }
  }
  if (!response) {
    jsonrpc_request_free(request);
    return rpc_build_fallback_error();
  }

  if (request && request->id) {
    response->id = cJSON_Duplicate(request->id, true);
  }

  char *response_str = jsonrpc_response_to_json(response);
  jsonrpc_response_free(response);
  jsonrpc_request_free(request);
  if (!response_str) {
    return rpc_build_fallback_error();
  }
  return response_str;
}

#include "rpc_m_light.h"

#include "config.h"
#include "gpio_led.h"
#include "gpio_rgb.h"

JsonRpcResponse *m_light_led_set(cJSON *params) {

  cJSON *gpio_json = cJSON_GetObjectItem(params, "gpio");
  cJSON *state_json = cJSON_GetObjectItem(params, "state");
  if (!cJSON_IsNumber(gpio_json) || !cJSON_IsNumber(state_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'gpio' and 'state' must be numbers", JSONRPC_INVALID_PARAMS);
  }
  int gpio_num = gpio_json->valueint; // must be one of 4,5,0
  int state = state_json->valueint;   // must be 0 or 1
  if ((gpio_num != GPIO_LIGHT_4 && gpio_num != GPIO_LIGHT_5 && gpio_num != GPIO_LIGHT_0) || (state != 0 && state != 1)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'gpio' must be 4, 5, or 0 and 'state' must be 0 or 1", JSONRPC_INVALID_PARAMS);
  }

  esp_err_t err = gpio_led_set(gpio_num, state);
  if (err != ESP_OK) {
    return jsonrpc_response_create(NULL, "Failed to set light state", JSONRPC_INTERNAL_ERROR);
  }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddNumberToObject(result, "gpio", gpio_num);
  cJSON_AddNumberToObject(result, "state", state);
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_light_rgb_set(cJSON *params) {

  cJSON *r_json = cJSON_GetObjectItem(params, "r");
  cJSON *g_json = cJSON_GetObjectItem(params, "g");
  cJSON *b_json = cJSON_GetObjectItem(params, "b");
  cJSON *on_json = cJSON_GetObjectItem(params, "on");
  if (!cJSON_IsNumber(r_json) || !cJSON_IsNumber(g_json) || !cJSON_IsNumber(b_json) || !cJSON_IsBool(on_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'r', 'g', 'b' must be numbers and 'on' must be a boolean", JSONRPC_INVALID_PARAMS);
  }
  int r = r_json->valueint;
  int g = g_json->valueint;
  int b = b_json->valueint;
  bool on = cJSON_IsTrue(on_json);
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
    return jsonrpc_response_create(NULL, "Invalid params: 'r', 'g', 'b' must be in the range 0-255", JSONRPC_INVALID_PARAMS);
  }
  // gpio_rgb_set
  gpio_rgb_set(on, r, g, b);
  cJSON *result = cJSON_CreateObject();
  cJSON_AddNumberToObject(result, "r", r);
  cJSON_AddNumberToObject(result, "g", g);
  cJSON_AddNumberToObject(result, "b", b);
  cJSON_AddBoolToObject(result, "on", on);
  return jsonrpc_response_create(result, NULL, 0);
}

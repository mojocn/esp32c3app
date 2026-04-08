#include "rpc_m_display.h"

#include "esp_log.h"
#include "esp_random.h"
#include "max7219.h"

JsonRpcResponse *m_display_effect(cJSON *params) {
  int n = 0;
  if (!params) {
    n = (int)esp_random();
  } else {
    cJSON *n_json = cJSON_GetObjectItem(params, "n");
    if (!cJSON_IsNumber(n_json)) {
      n = (int)esp_random();
    } else {
      n = n_json->valueint;
    }
  }

  max7219_show_effect(n);

  cJSON *result = cJSON_CreateObject();
  cJSON_AddNumberToObject(result, "effect", n);
  return jsonrpc_response_create(result, NULL, 0);
}

#include "rpc_m_ir.h"

#include "m5stack_unit_ir.h"

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

#include "rpc_m_cron.h"

#include "cron_engine.h"

JsonRpcResponse *m_cron_list(cJSON *params) {
  (void)params;
  cJSON *result = cron_job_list();
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_cron_get(cJSON *params) {
  cJSON *id_json = cJSON_GetObjectItem(params, "id");
  if (!cJSON_IsNumber(id_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'id' (number) is required", JSONRPC_INVALID_PARAMS);
  }

  cJSON *result = cron_job_get((uint8_t)id_json->valueint);
  if (!result) {
    return jsonrpc_response_create(NULL, "Job not found", JSONRPC_INVALID_PARAMS);
  }
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_cron_create(cJSON *params) {
  cJSON *expr_json = cJSON_GetObjectItem(params, "expression");
  cJSON *method_json = cJSON_GetObjectItem(params, "method");
  if (!cJSON_IsString(expr_json) || !expr_json->valuestring || !cJSON_IsString(method_json) || !method_json->valuestring) {
    return jsonrpc_response_create(NULL, "Invalid params: 'expression' and 'method' (strings) are required", JSONRPC_INVALID_PARAMS);
  }

  if (!cron_expr_validate(expr_json->valuestring)) {
    return jsonrpc_response_create(NULL, "Invalid cron expression", JSONRPC_INVALID_PARAMS);
  }

  cJSON *params_json = cJSON_GetObjectItem(params, "params");
  cJSON *enabled_json = cJSON_GetObjectItem(params, "enabled");
  bool enabled = cJSON_IsBool(enabled_json) ? cJSON_IsTrue(enabled_json) : true;

  int id = cron_job_create(expr_json->valuestring, method_json->valuestring, params_json, enabled);
  if (id == -1) {
    return jsonrpc_response_create(NULL, "Invalid cron expression", JSONRPC_INVALID_PARAMS);
  }
  if (id == -2) {
    return jsonrpc_response_create(NULL, "Max cron jobs reached", JSONRPC_INTERNAL_ERROR);
  }

  cJSON *result = cron_job_get((uint8_t)id);
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_cron_update(cJSON *params) {
  cJSON *id_json = cJSON_GetObjectItem(params, "id");
  if (!cJSON_IsNumber(id_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'id' (number) is required", JSONRPC_INVALID_PARAMS);
  }
  uint8_t id = (uint8_t)id_json->valueint;

  cJSON *expr_json = cJSON_GetObjectItem(params, "expression");
  cJSON *method_json = cJSON_GetObjectItem(params, "method");
  cJSON *params_json = cJSON_GetObjectItem(params, "params");
  cJSON *enabled_json = cJSON_GetObjectItem(params, "enabled");

  const char *expr = (cJSON_IsString(expr_json) && expr_json->valuestring) ? expr_json->valuestring : NULL;
  const char *method = (cJSON_IsString(method_json) && method_json->valuestring) ? method_json->valuestring : NULL;
  bool enabled = cJSON_IsBool(enabled_json) ? cJSON_IsTrue(enabled_json) : true;

  if (!cron_job_update(id, expr, method, params_json, enabled)) {
    return jsonrpc_response_create(NULL, "Job not found or invalid expression", JSONRPC_INVALID_PARAMS);
  }

  cJSON *result = cron_job_get(id);
  return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_cron_delete(cJSON *params) {
  cJSON *id_json = cJSON_GetObjectItem(params, "id");
  if (!cJSON_IsNumber(id_json)) {
    return jsonrpc_response_create(NULL, "Invalid params: 'id' (number) is required", JSONRPC_INVALID_PARAMS);
  }

  if (!cron_job_delete((uint8_t)id_json->valueint)) {
    return jsonrpc_response_create(NULL, "Job not found", JSONRPC_INVALID_PARAMS);
  }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "status", "ok");
  return jsonrpc_response_create(result, NULL, 0);
}

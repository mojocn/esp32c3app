#include "rpc_json.h"

#include "cJSON.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

JsonRpcRequest *jsonrpc_parse_request(const char *json) {
    if (!json) {
        return NULL;
    }
    cJSON *root = cJSON_Parse(json);
    if (!root)
        return NULL;

    cJSON *jsonrpc_item = cJSON_GetObjectItem(root, "jsonrpc");
    cJSON *method_item = cJSON_GetObjectItem(root, "method");

    if (!jsonrpc_item || !method_item || !jsonrpc_item->valuestring || !method_item->valuestring) {
        cJSON_Delete(root);
        return NULL;
    }

    JsonRpcRequest *req = calloc(1, sizeof(JsonRpcRequest));
    if (!req) {
        cJSON_Delete(root);
        return NULL;
    }
    snprintf(req->jsonrpc, sizeof(req->jsonrpc), "%s", jsonrpc_item->valuestring);
    req->method = strdup(method_item->valuestring);
    if (!req->method) {
        cJSON_Delete(root);
        free(req);
        return NULL;
    }

    req->id = cJSON_Duplicate(cJSON_GetObjectItem(root, "id"), true);
    req->params = cJSON_Duplicate(cJSON_GetObjectItem(root, "params"), true);

    cJSON_Delete(root);
    return req;
}

void jsonrpc_request_free(void *req_ptr) {
    if (!req_ptr) {
        return;
    }
    JsonRpcRequest *req = (JsonRpcRequest *)req_ptr;
    free(req->method);
    if (req->id) {
        cJSON_Delete(req->id);
    }
    if (req->params) {
        cJSON_Delete(req->params);
    }
    free(req);
}

void jsonrpc_response_free(JsonRpcResponse *resp) {
    if (!resp) {
        return;
    }
    if (resp->id) {
        cJSON_Delete(resp->id);
    }
    if (resp->result) {
        cJSON_Delete(resp->result);
    }
    if (resp->error) {
        free(resp->error->message);
        free(resp->error);
    }
    free(resp);
}

JsonRpcResponse *jsonrpc_response_create(cJSON *result, const char *err_msg, int err_code) {
    JsonRpcResponse *resp = calloc(1, sizeof(JsonRpcResponse));
    if (!resp) {
        if (result) {
            cJSON_Delete(result);
        }
        return NULL;
    }
    strcpy(resp->jsonrpc, "2.0");
    resp->result = result;
    resp->id = NULL;
    if (err_msg) {
        resp->error = calloc(1, sizeof(JsonRpcError));
        if (!resp->error) {
            if (result) {
                cJSON_Delete(result);
            }
            free(resp);
            return NULL;
        }
        resp->error->code = err_code;
        resp->error->message = strdup(err_msg);
        if (!resp->error->message) {
            if (result) {
                cJSON_Delete(result);
            }
            free(resp->error);
            free(resp);
            return NULL;
        }
    } else {
        resp->error = NULL;
    }
    return resp;
}

char *jsonrpc_response_to_json(JsonRpcResponse *resp) {
    if (!resp)
        return NULL;

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");

    if (resp->id) {
        cJSON_AddItemToObject(root, "id", cJSON_Duplicate(resp->id, true));
    }

    if (resp->error) {
        cJSON *error_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(error_obj, "code", resp->error->code);
        cJSON_AddStringToObject(error_obj, "message", resp->error->message);
        cJSON_AddItemToObject(root, "error", error_obj);
    } else if (resp->result) {
        cJSON_AddItemToObject(root, "result", cJSON_Duplicate(resp->result, true));
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

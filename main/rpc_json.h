#ifndef RPC_JSON
#define RPC_JSON

#include "cJSON.h"

/* JSON-RPC 2.0 Error Codes */
#define JSONRPC_PARSE_ERROR -32700
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS -32602
#define JSONRPC_INTERNAL_ERROR -32603

typedef struct {
    int code;
    char *message;
} JsonRpcError;

typedef struct {
    char jsonrpc[8];
    cJSON *id;
    cJSON *result;
    JsonRpcError *error;
} JsonRpcResponse;

typedef struct {
    char jsonrpc[8]; // default to "2.0"
    char *method;
    cJSON *id;
    cJSON *params;
} JsonRpcRequest;

JsonRpcRequest *jsonrpc_parse_request(const char *json);
char *jsonrpc_response_to_json(JsonRpcResponse *resp);
JsonRpcResponse *jsonrpc_response_create(cJSON *result, const char *err_msg, int err_code);
void jsonrpc_response_free(JsonRpcResponse *resp);
void jsonrpc_request_free(void *req);

#endif // RPC_JSON

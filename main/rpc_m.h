
#ifndef RPC_M_H
#define RPC_M_H

#include "cJSON.h"
#include "rpc_json.h"

typedef JsonRpcResponse *(*rpc_handler_t)(cJSON *params);

char *rpc_process_request(const char *request_str);

#endif // RPC_M_H

#ifndef RPC_M_CONFIG_H
#define RPC_M_CONFIG_H

#include "cJSON.h"
#include "rpc_json.h"

JsonRpcResponse *m_config_set(cJSON *params);
JsonRpcResponse *m_config_get(cJSON *params);

#endif // RPC_M_CONFIG_H

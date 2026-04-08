#ifndef RPC_M_LIGHT_H
#define RPC_M_LIGHT_H

#include "cJSON.h"
#include "rpc_json.h"

JsonRpcResponse *m_light_led_set(cJSON *params);
JsonRpcResponse *m_light_rgb_set(cJSON *params);

#endif // RPC_M_LIGHT_H

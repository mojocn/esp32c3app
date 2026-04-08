
#ifndef RPC_M_WIFI_H
#define RPC_M_WIFI_H

#include "cJSON.h"
#include "rpc_json.h"

JsonRpcResponse *m_wifi_sta_set(cJSON *params);
JsonRpcResponse *m_wifi_ap_set(cJSON *params);
JsonRpcResponse *m_wifi_info(cJSON *params);

#endif // RPC_M_WIFI_H

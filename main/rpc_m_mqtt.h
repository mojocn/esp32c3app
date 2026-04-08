#ifndef RPC_M_MQTT_H
#define RPC_M_MQTT_H

#include "rpc_m.h"

JsonRpcResponse *m_mqtt_set(cJSON *params);
JsonRpcResponse *m_mqtt_info(cJSON *params);

#endif // RPC_M_MQTT_H

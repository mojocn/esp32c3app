#ifndef RPC_M_CRON_H
#define RPC_M_CRON_H

#include "cJSON.h"
#include "rpc_json.h"

JsonRpcResponse *m_cron_list(cJSON *params);
JsonRpcResponse *m_cron_get(cJSON *params);
JsonRpcResponse *m_cron_create(cJSON *params);
JsonRpcResponse *m_cron_update(cJSON *params);
JsonRpcResponse *m_cron_delete(cJSON *params);

#endif // RPC_M_CRON_H

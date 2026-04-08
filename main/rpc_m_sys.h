
#ifndef RPC_M_SYS_H
#define RPC_M_SYS_H

#include "cJSON.h"
#include "rpc_json.h"

JsonRpcResponse *m_sys_info(cJSON *params);
JsonRpcResponse *m_sys_ota(cJSON *params);
JsonRpcResponse *m_sys_reboot(cJSON *params);
JsonRpcResponse *m_sys_factory(cJSON *params);

#endif // RPC_M_SYS_H

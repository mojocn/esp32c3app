#ifndef RPC_M_IR_H
#define RPC_M_IR_H

#include "cJSON.h"
#include "rpc_json.h"

JsonRpcResponse *m_ir_send(cJSON *params);
JsonRpcResponse *m_ir_record(cJSON *params);
JsonRpcResponse *m_ir_list(cJSON *params);

#endif // RPC_M_IR_H

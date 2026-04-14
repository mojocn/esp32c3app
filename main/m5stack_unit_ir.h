#ifndef M5STACK_UNIT_IR_H
#define M5STACK_UNIT_IR_H

#include <stdbool.h>
#include <stdint.h>

void ir_init(void);
void ir_send_nec(uint8_t addr, uint8_t cmd);
bool ir_record_nec(uint8_t *addr, uint8_t *cmd, uint32_t timeout_ms);

#endif // M5STACK_UNIT_IR_H

#include "m5stack_unit_ir.h"

#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

#define TAG "M5_IR"

// 引脚定义
#define IR_TX_GPIO GPIO_NUM_4
#define IR_RX_GPIO GPIO_NUM_5

// NEC 协议
#define NEC_FREQ 38000
#define NEC_DUTY 0.33f
#define RMT_RES 1000000 // 1MHz → 1 tick = 1µs

// NEC timings (µs = ticks at 1MHz)
#define NEC_LEADER_H 9000
#define NEC_LEADER_L 4500
#define NEC_BIT_H 560
#define NEC_BIT0_L 560
#define NEC_BIT1_L 1680
#define NEC_BURST 560

// 发送句柄
static rmt_channel_handle_t tx_chan = NULL;
static rmt_encoder_handle_t nec_encoder = NULL;

// 接收缓冲区
#define RX_BUF_SZ 256
static uint8_t rx_buf[RX_BUF_SZ];

// 接收句柄
static rmt_channel_handle_t rx_chan = NULL;

// 接收解码队列
typedef struct {
  uint8_t addr;
  uint8_t cmd;
} nec_result_t;
static QueueHandle_t nec_queue = NULL;

// ---------------------------
// NEC 解码（ISR 安全）
// ---------------------------
static inline bool is_near(uint32_t val, uint32_t target, uint32_t tol) { return val >= target - tol && val <= target + tol; }

static bool decode_nec(const rmt_symbol_word_t *syms, size_t n, uint8_t *addr, uint8_t *cmd) {
  if (n < 33) return false;
  // Leader: ~9000µs H + ~4500µs L
  if (!is_near(syms[0].duration0, 9000, 1000) || !is_near(syms[0].duration1, 4500, 1000)) return false;
  uint32_t bits = 0;
  for (int i = 1; i <= 32; i++) {
    if (!is_near(syms[i].duration0, 560, 200)) return false;
    if (is_near(syms[i].duration1, 1680, 300))
      bits |= (1u << (i - 1));
    else if (!is_near(syms[i].duration1, 560, 200))
      return false;
  }
  uint8_t a = bits & 0xFF;
  uint8_t na = (bits >> 8) & 0xFF;
  uint8_t c = (bits >> 16) & 0xFF;
  uint8_t nc = (bits >> 24) & 0xFF;
  if ((a ^ na) != 0xFF || (c ^ nc) != 0xFF) return false;
  *addr = a;
  *cmd = c;
  return true;
}

// ---------------------------
// 发送初始化
// ---------------------------
static void tx_init(void) {
  rmt_tx_channel_config_t cfg = {
      .gpio_num = IR_TX_GPIO,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = RMT_RES,
      .mem_block_symbols = 48,
      .trans_queue_depth = 4,
      .flags.invert_out = false,
  };
  ESP_ERROR_CHECK(rmt_new_tx_channel(&cfg, &tx_chan));

  // 载波 38kHz
  rmt_carrier_config_t carrier = {.frequency_hz = NEC_FREQ, .duty_cycle = NEC_DUTY, .flags.polarity_active_low = false};
  ESP_ERROR_CHECK(rmt_apply_carrier(tx_chan, &carrier));

  // 使用 copy encoder，symbol 在发送时手动构建
  rmt_copy_encoder_config_t copy_cfg = {};
  ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &nec_encoder));

  ESP_ERROR_CHECK(rmt_enable(tx_chan));
  ESP_LOGI(TAG, "TX init OK");
}

// ---------------------------
// 发送 NEC 命令
// ---------------------------
void ir_send_nec(uint8_t addr, uint8_t cmd) {
  // NEC frame: 1 leader + 32 data bits + 1 end burst = 34 symbols
  rmt_symbol_word_t syms[34];
  int idx = 0;

  // 引导码: 9ms H + 4.5ms L
  syms[idx++] = (rmt_symbol_word_t){.duration0 = NEC_LEADER_H, .level0 = 1, .duration1 = NEC_LEADER_L, .level1 = 0};

  // 32 bits: addr, ~addr, cmd, ~cmd (LSB first)
  uint8_t data[4] = {addr, (uint8_t)~addr, cmd, (uint8_t)~cmd};
  for (int b = 0; b < 4; b++) {
    for (int bit = 0; bit < 8; bit++) {
      bool one = (data[b] >> bit) & 1;
      syms[idx++] = (rmt_symbol_word_t){.duration0 = NEC_BIT_H, .level0 = 1, .duration1 = one ? NEC_BIT1_L : NEC_BIT0_L, .level1 = 0};
    }
  }

  // 结束脉冲
  syms[idx++] = (rmt_symbol_word_t){.duration0 = NEC_BURST, .level0 = 1, .duration1 = 0, .level1 = 0};

  rmt_transmit_config_t tx_cfg = {.loop_count = 0};
  ESP_ERROR_CHECK(rmt_transmit(tx_chan, nec_encoder, syms, idx * sizeof(rmt_symbol_word_t), &tx_cfg));
  ESP_LOGI(TAG, "Send NEC: addr=0x%02X cmd=0x%02X", addr, cmd);
}

// ---------------------------
// 接收回调（解析 NEC）
// ---------------------------
static bool rx_done(rmt_channel_handle_t chan, const rmt_rx_done_event_data_t *edata, void *ctx) {
  // NOTE: This runs in ISR context — no logging, no blocking calls allowed.
  BaseType_t need_wake = pdFALSE;

  nec_result_t result;
  if (nec_queue && decode_nec(edata->received_symbols, edata->num_symbols, &result.addr, &result.cmd)) {
    xQueueSendFromISR(nec_queue, &result, &need_wake);
  }

  // 重启接收
  rmt_receive(chan, rx_buf, RX_BUF_SZ, &(rmt_receive_config_t){.signal_range_min_ns = 1000, .signal_range_max_ns = 20000000});
  return need_wake;
}

// ---------------------------
// 接收初始化
// ---------------------------
static void rx_init(void) {
  rmt_rx_channel_config_t cfg = {
      .gpio_num = IR_RX_GPIO,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = RMT_RES,
      .mem_block_symbols = 48,
      .flags.invert_in = false,
  };
  esp_err_t ret = rmt_new_rx_channel(&cfg, &rx_chan);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create RX channel: %s", esp_err_to_name(ret));
    return;
  }

  rmt_rx_event_callbacks_t cbs = {.on_recv_done = rx_done};
  ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_chan, &cbs, NULL));

  ESP_ERROR_CHECK(rmt_enable(rx_chan));
  ESP_ERROR_CHECK(rmt_receive(rx_chan, rx_buf, RX_BUF_SZ, &(rmt_receive_config_t){.signal_range_min_ns = 1000, .signal_range_max_ns = 20000000}));
  ESP_LOGI(TAG, "RX init OK");
}

// ---------------------------
// 主函数
// ---------------------------
// void app_main(void) {
//   tx_init();
//   rx_init();
//   vTaskDelay(pdMS_TO_TICKS(1000));

//   // 测试发送
//   ir_send_nec(0x00, 0x12);

//   while (1) {
//     vTaskDelay(pdMS_TO_TICKS(5000));
//     ir_send_nec(0x00, 0x12);
//   }
// }

// ---------------------------
// 公开初始化
// ---------------------------
static void ir_rx_task(void *arg) {
  nec_result_t r;
  while (1) {
    if (xQueueReceive(nec_queue, &r, portMAX_DELAY)) {
      ESP_LOGI(TAG, "Received NEC: addr=0x%02X cmd=0x%02X", r.addr, r.cmd);
    }
  }
}

void ir_init(void) {
  nec_queue = xQueueCreate(8, sizeof(nec_result_t));
  tx_init();
  rx_init();
  xTaskCreate(ir_rx_task, "ir_rx", 2048, NULL, 5, NULL);
}

#include "gpio_led.h"

#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
/* LED Strip Configuration */
#define RGB_LED_GPIO 8
#define LED_NUM 1

static const char *TAG = "GPIO";

void gpio_led_init(void) {
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << GPIO_LIGHT_4) | (1ULL << GPIO_LIGHT_5) | (1ULL << GPIO_LIGHT_0),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  gpio_set_level(GPIO_LIGHT_4, 0);
  gpio_set_level(GPIO_LIGHT_5, 0);
  gpio_set_level(GPIO_LIGHT_0, 0);
  ESP_LOGI(TAG, "GPIOs %d, %d, %d initialized as output", GPIO_LIGHT_4, GPIO_LIGHT_5, GPIO_LIGHT_0);
}

esp_err_t gpio_led_set(uint8_t gpio_num, uint8_t state) {
  if (state != 0 && state != 1) {
    return ESP_ERR_INVALID_ARG;
  }
  if (gpio_num != GPIO_LIGHT_4 && gpio_num != GPIO_LIGHT_5 && gpio_num != GPIO_LIGHT_0) {
    return ESP_ERR_INVALID_ARG;
  }
  gpio_set_level(gpio_num, state);
  return ESP_OK;
}

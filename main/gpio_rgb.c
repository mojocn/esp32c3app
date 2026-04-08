#include "gpio_rgb.h"

#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

/* LED Strip Configuration */

static const char *TAG = "RGB";

static led_strip_handle_t led_strip = NULL;

void gpio_rgb_set(uint8_t on_off, uint8_t r, uint8_t g, uint8_t b) {
  if (on_off) {
    led_strip_set_pixel(led_strip, 0, r, g, b);
  } else {
    led_strip_clear(led_strip);
  }
  led_strip_refresh(led_strip);
}

/* Initialize LED Strip */
void gpio_rgb_init(void) {
  ESP_LOGI(TAG, "Initializing RGB LED on GPIO %d", RGB_LED_GPIO);

  led_strip_config_t strip_config = {
      .strip_gpio_num = RGB_LED_GPIO,
      .max_leds = LED_NUM,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB,
      .led_model = LED_MODEL_WS2812,
      .flags.invert_out = false,
  };
  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, /* 10 MHz */
      .flags.with_dma = false,
  };
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  led_strip_clear(led_strip);
  ESP_LOGI(TAG, "RGB LED initialized successfully");
  gpio_rgb_set(1, 1, 1, 1); // Ensure LED is off at startup
}

#ifndef GPIO_LED_H
#define GPIO_LED_H

#include "esp_err.h"

#include <stdint.h>

#define GPIO_LIGHT_0 0

void gpio_led_init(void);

esp_err_t gpio_led_set(uint8_t gpio_num, uint8_t state);

void gpio_led_blink(void);
#endif // GPIO_LED_H

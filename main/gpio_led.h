#ifndef GPIO_LED_H
#define GPIO_LED_H

#include "esp_err.h"

#include <stdint.h>

#define GPIO_LIGHT_4 4
#define GPIO_LIGHT_5 5
#define GPIO_LIGHT_0 0

void gpio_led_init(void);

esp_err_t gpio_led_set(uint8_t gpio_num, uint8_t state);

#endif // GPIO_LED_H

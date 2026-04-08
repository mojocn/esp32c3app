#ifndef GPIO_8_RGB_H
#define GPIO_8_RGB_H

#include <stdint.h>

#define RGB_LED_GPIO 8
#define LED_NUM 1

void gpio_rgb_init(void);
void gpio_rgb_set(uint8_t on_off, uint8_t r, uint8_t g, uint8_t b);

#endif // GPIO_8_RGB_H

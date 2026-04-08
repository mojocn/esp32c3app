#ifndef MAX7219_H
#define MAX7219_H

#include "esp_err.h"

#include <stdint.h>

/* Pin definitions */
#define MAX7219_CS_PIN 19
#define MAX7219_CLK_PIN 1
#define MAX7219_DIN_PIN 18

/**
 * @brief Initialize MAX7219 8x8 LED matrix and start demo task
 */
void max7219_init(void);

/**
 * @brief Send raw 8-byte framebuffer to the display
 * @param fb 8 bytes, fb[0] = row 1 … fb[7] = row 8
 */
void max7219_display(const uint8_t fb[8]);

/**
 * @brief Clear the display (all LEDs off)
 */
void max7219_clear(void);

/**
 * @brief Set display brightness
 * @param level 0 (dimmest) to 15 (brightest)
 */
void max7219_set_brightness(uint8_t level);

/**
 * @brief Run a demo effect by number (1-16) asynchronously in a background task.
 * If an effect is already running it will be stopped and the new one started.
 * This function returns immediately.
 * @param n Effect number (1..16). Out-of-range values are ignored.
 */
void max7219_show_effect(int n);
#endif // MAX7219_H

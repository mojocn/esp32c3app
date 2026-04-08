#ifndef DHT11_H
#define DHT11_H

#include "esp_err.h"

#include <stdint.h>

#define DHT11_GPIO 2

typedef struct {
    float temperature;
    float humidity;
} dht11_reading_t;

/**
 * @brief Initialize the DHT11 sensor and start periodic reading task
 */
void dht11_init(void);

/**
 * @brief Read temperature and humidity from DHT11
 * @param[out] reading Pointer to store the reading
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on communication error,
 *         ESP_ERR_INVALID_CRC on checksum mismatch
 */
esp_err_t dht11_read(dht11_reading_t *reading);

/**
 * @brief Get the last successful reading (non-blocking)
 * @param[out] reading Pointer to store the cached reading
 * @return ESP_OK if a valid reading is available, ESP_ERR_NOT_FOUND if no reading yet
 */
esp_err_t dht11_get_last_reading(dht11_reading_t *reading);

#endif // DHT11_H

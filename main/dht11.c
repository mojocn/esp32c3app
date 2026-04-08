#include "dht11.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

#include <string.h>

static const char *TAG = "DHT11";

/* Cached last-good reading */
static dht11_reading_t s_last_reading;
static bool s_has_reading = false;

/* Wait for the data pin to reach a given level, return elapsed microseconds.
   Returns -1 on timeout. */
static int wait_for_level(gpio_num_t pin, int level, int timeout_us) {
    int elapsed = 0;
    while (gpio_get_level(pin) != level) {
        if (elapsed >= timeout_us) {
            return -1;
        }
        ets_delay_us(1);
        elapsed++;
    }
    return elapsed;
}

esp_err_t dht11_read(dht11_reading_t *reading) {
    const gpio_num_t pin = DHT11_GPIO;
    uint8_t data[5] = {0};

    /* ---- Start signal: pull low >= 18 ms, then release ---- */
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    ets_delay_us(20000); /* 20 ms */
    gpio_set_level(pin, 1);
    ets_delay_us(30);
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    /* Disable interrupts for the entire timing-critical read.
       FreeRTOS task switches and ISRs can disrupt the µs-level
       pulse measurements and cause false timeouts. */
    portDISABLE_INTERRUPTS();

    /* ---- DHT11 response: low ~80 us, then high ~80 us ---- */
    if (wait_for_level(pin, 0, 100) < 0) {
        portENABLE_INTERRUPTS();
        return ESP_ERR_TIMEOUT;
    }
    if (wait_for_level(pin, 1, 100) < 0) {
        portENABLE_INTERRUPTS();
        return ESP_ERR_TIMEOUT;
    }
    if (wait_for_level(pin, 0, 100) < 0) {
        portENABLE_INTERRUPTS();
        return ESP_ERR_TIMEOUT;
    }

    /* ---- Read 40 bits (5 bytes) ---- */
    for (int i = 0; i < 40; i++) {
        /* Each bit starts with ~50 us low */
        if (wait_for_level(pin, 1, 70) < 0) {
            portENABLE_INTERRUPTS();
            return ESP_ERR_TIMEOUT;
        }
        /* Duration of high level determines bit value:
           ~26-28 us = 0, ~70 us = 1 */
        int high_us = wait_for_level(pin, 0, 100);
        if (high_us < 0) {
            portENABLE_INTERRUPTS();
            return ESP_ERR_TIMEOUT;
        }
        data[i / 8] <<= 1;
        if (high_us > 40) {
            data[i / 8] |= 1;
        }
    }

    portENABLE_INTERRUPTS();

    /* ---- Checksum verification ---- */
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGW(TAG, "Checksum mismatch: %d+%d+%d+%d=%d, expected %d", data[0], data[1], data[2], data[3], checksum,
                 data[4]);
        return ESP_ERR_INVALID_CRC;
    }

    /* DHT11: data[0]=humidity_int, data[1]=humidity_dec,
              data[2]=temp_int, data[3]=temp_dec */
    reading->humidity = data[0] + data[1] * 0.1f;
    reading->temperature = data[2] + data[3] * 0.1f;

    ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", reading->temperature, reading->humidity);
    return ESP_OK;
}

esp_err_t dht11_get_last_reading(dht11_reading_t *reading) {
    if (!s_has_reading) {
        return ESP_ERR_NOT_FOUND;
    }
    *reading = s_last_reading;
    return ESP_OK;
}

/* Periodic reading task — DHT11 needs >= 1 s between reads */
static void dht11_task(void *pvParameters) {
    /* Initial delay to let sensor stabilize after power-on */
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        dht11_reading_t reading;
        esp_err_t err = dht11_read(&reading);
        if (err == ESP_OK) {
            s_last_reading = reading;
            s_has_reading = true;
        } else {
            ESP_LOGW(TAG, "Read failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(5000)); /* Read every 5 seconds */
    }
}

void dht11_init(void) {
    ESP_LOGI(TAG, "Initializing DHT11 on GPIO %d", DHT11_GPIO);

    /* Configure GPIO with pull-up (DHT11 data line is open-drain) */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT11_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    xTaskCreate(dht11_task, "dht11_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "DHT11 initialized, reading every 5 seconds");
}

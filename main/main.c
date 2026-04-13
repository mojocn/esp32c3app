/* JSON-RPC 2.0 HTTP/BLE/MQTT Server for ESP32-C3 */

#include "app_event.h"
#include "ble_gatt_server.h"
#include "buzzer.h"
#include "config.h"
#include "cron_engine.h"
#include "dht11.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_led.h"
#include "gpio_rgb.h"
#include "http_server.h"
#include "m5stack_unit_ir.h"
#include "max7219.h"
#include "mqtt_manager.h"
#include "nvs_flash.h"
#include "wifi_manager.h"

#include <stdio.h>

static const char *TAG = "MAIN";

void app_main(void) {
  /* Initialize NVS */
  AppConfig *config = config_init();
  if (!config) {
    ESP_LOGE(TAG, "Failed to initialize configuration");
    return;
  }

  app_event_init();
  wifi_init();

  wifi_config_apply(config);

  /* Initialize GPIO */
  gpio_led_init();

  /* Initialize RGB LED */
  gpio_rgb_init();

  /* Initialize DHT11 temperature & humidity sensor */
  dht11_init();

  /* Initialize MAX7219 8x8 LED matrix (with demo) */
  max7219_init();

  /* Initialize buzzer and run demo (4x on/off every 5s) */
  buzzer_init();
  // buzzer_demo();

  /* Initialize IR transceiver */
  ir_init();

  /* Initialize WiFi (must be before BLE to allow BLE provisioning if needed) */

  /* Start HTTP Server */
  http_server_start();

  /* Start cron engine (SNTP + scheduler task) */
  cron_engine_init();

  /* Start BLE GATT server (JSON-RPC over NUS-compatible service) */
  ESP_LOGI(TAG, "Starting BLE GATT server...");
  if (ble_gatt_server_init() == ESP_OK) {
    ESP_LOGI(TAG, "BLE GATT server started (device name: %s)", device_name());
  } else {
    ESP_LOGE(TAG, "Failed to start BLE GATT server");
  }

  config_free(config);
}

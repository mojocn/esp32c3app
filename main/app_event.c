
#include "app_event.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "mqtt_manager.h"

const char *TAG = "APP_EVENT";

const int WIFI_CONN_MAX_RETRY = 5;
int wifi_sta_retry = 0;

static void event_handler_all(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to connect to WiFi: %s", esp_err_to_name(err));
    }
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (wifi_sta_retry < WIFI_CONN_MAX_RETRY) {
      esp_err_t err = esp_wifi_connect();
      if (err == ESP_OK) {
        wifi_sta_retry++;
      }
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    wifi_sta_retry = 0;
    // start mqtt
    mqtt_manager_start();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
    ESP_LOGI(TAG, "IP lost - stopping MQTT manager");
    mqtt_manager_stop();
  } else if (event_base == ESP_HTTP_SERVER_EVENT) {
    ESP_LOGD(TAG, "Ignored HTTP server event: id=%d", event_id);
  } else {
    ESP_LOGI(TAG, "Unhandled event: base=%s id=%d", event_base ? event_base : "(null)", event_id);
  }
}

void app_event_init() {
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler_all, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler_all, NULL, NULL));
}

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"

/**
 * @brief Start an OTA firmware update from the given HTTP/HTTPS URL.
 *
 * The update runs in a dedicated FreeRTOS task. On success the device
 * reboots automatically. On failure the task exits and logs the error.
 *
 * @param url  Firmware binary URL (http:// or https://)
 * @return ESP_OK if the update task was launched, ESP_FAIL otherwise
 */
esp_err_t ota_manager_start(const char *url);

#endif // OTA_MANAGER_H

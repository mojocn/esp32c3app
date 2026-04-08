#include "ota_manager.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "OTA";

/* Maximum URL length for OTA */
#define OTA_URL_MAX_LEN 256

typedef struct {
    char url[OTA_URL_MAX_LEN];
} ota_task_args_t;

/* ------------------------------------------------------------------ */
/* OTA task                                                             */
/* ------------------------------------------------------------------ */

static void ota_task(void *arg) {
    ota_task_args_t *args = (ota_task_args_t *)arg;

    ESP_LOGI(TAG, "Starting OTA update from: %s", args->url);

    esp_http_client_config_t http_cfg = {
        .url = args->url,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
        /* For production use a proper CA cert;
           cert_pem = (char *)server_cert_pem_start; */
        .skip_cert_common_name_check = false,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        free(args);
        vTaskDelete(NULL);
        return;
    }

    esp_app_desc_t app_desc;
    if (esp_https_ota_get_img_desc(ota_handle, &app_desc) == ESP_OK) {
        ESP_LOGI(TAG, "Incoming firmware: %s %s", app_desc.project_name, app_desc.version);
    }

    while (true) {
        err = esp_https_ota_perform(ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        int received = esp_https_ota_get_image_len_read(ota_handle);
        ESP_LOGD(TAG, "OTA progress: %d bytes received", received);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA perform failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(ota_handle);
        free(args);
        vTaskDelete(NULL);
        return;
    }

    bool image_valid = esp_https_ota_is_complete_data_received(ota_handle);
    if (!image_valid) {
        ESP_LOGE(TAG, "OTA image incomplete");
        esp_https_ota_abort(ota_handle);
        free(args);
        vTaskDelete(NULL);
        return;
    }

    err = esp_https_ota_finish(ota_handle);
    free(args);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA succeeded. Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(err));
    }

    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

esp_err_t ota_manager_start(const char *url) {
    if (!url || strlen(url) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(url) >= OTA_URL_MAX_LEN) {
        ESP_LOGE(TAG, "OTA URL too long (max %d chars)", OTA_URL_MAX_LEN - 1);
        return ESP_ERR_INVALID_ARG;
    }

    ota_task_args_t *args = malloc(sizeof(ota_task_args_t));
    if (!args) {
        return ESP_ERR_NO_MEM;
    }
    strncpy(args->url, url, OTA_URL_MAX_LEN - 1);
    args->url[OTA_URL_MAX_LEN - 1] = '\0';

    BaseType_t ret = xTaskCreate(ota_task, "ota_task", 8192, args, 5, NULL);
    if (ret != pdPASS) {
        free(args);
        return ESP_FAIL;
    }

    return ESP_OK;
}

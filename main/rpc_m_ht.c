#include "rpc_m_ht.h"

#include "cJSON.h"
#include "dht11.h"
#include "esp_err.h"

JsonRpcResponse *m_ht_info(cJSON *params) {
    (void)params; // no params expected

    dht11_reading_t reading;
    esp_err_t err = dht11_get_last_reading(&reading);

    if (err == ESP_ERR_NOT_FOUND) {
        cJSON *result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "no_data");
        cJSON_AddStringToObject(result, "message", "DHT11 reading not ready yet");
        return jsonrpc_response_create(result, NULL, 0);
    } else if (err != ESP_OK) {
        return jsonrpc_response_create(NULL, "DHT11 read error", JSONRPC_INTERNAL_ERROR);
    }

    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "temperature", reading.temperature);
    cJSON_AddNumberToObject(result, "humidity", reading.humidity);
    cJSON_AddStringToObject(result, "unit_temp", "C");
    cJSON_AddStringToObject(result, "unit_humidity", "%");
    return jsonrpc_response_create(result, NULL, 0);
}

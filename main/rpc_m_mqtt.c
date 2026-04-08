#include "rpc_m_mqtt.h"

#include "config.h"
#include "mqtt_manager.h"
#include "rpc_json.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

JsonRpcResponse *m_mqtt_set(cJSON *params) {
    if (!cJSON_IsObject(params)) {
        return jsonrpc_response_create(NULL, "Invalid params: expected an object", JSONRPC_INVALID_PARAMS);
    }

    AppConfig *config = config_get();
    if (!config) {
        config = config_init();
        if (!config) {
            return jsonrpc_response_create(NULL, "Failed to load config", JSONRPC_INTERNAL_ERROR);
        }
    }

    cJSON *host_json = cJSON_GetObjectItem(params, "host");
    cJSON *port_json = cJSON_GetObjectItem(params, "port");
    cJSON *username_json = cJSON_GetObjectItem(params, "username");
    cJSON *password_json = cJSON_GetObjectItem(params, "password");

    if (host_json && !cJSON_IsString(host_json)) {
        config_free(config);
        return jsonrpc_response_create(NULL, "Invalid params: 'host' must be a string", JSONRPC_INVALID_PARAMS);
    }
    if (port_json && !cJSON_IsNumber(port_json)) {
        config_free(config);
        return jsonrpc_response_create(NULL, "Invalid params: 'port' must be a number", JSONRPC_INVALID_PARAMS);
    }
    if (username_json && !cJSON_IsString(username_json)) {
        config_free(config);
        return jsonrpc_response_create(NULL, "Invalid params: 'username' must be a string", JSONRPC_INVALID_PARAMS);
    }
    if (password_json && !cJSON_IsString(password_json)) {
        config_free(config);
        return jsonrpc_response_create(NULL, "Invalid params: 'password' must be a string", JSONRPC_INVALID_PARAMS);
    }

    if (host_json) {
        char *new_host = strdup(host_json->valuestring);
        if (!new_host) {
            config_free(config);
            return jsonrpc_response_create(NULL, "Out of memory", JSONRPC_INTERNAL_ERROR);
        }
        free(config->mqtt_broker_host);
        config->mqtt_broker_host = new_host;
    }

    if (port_json) {
        int port_val = port_json->valueint;
        if (port_val <= 0 || port_val > 65535) {
            config_free(config);
            return jsonrpc_response_create(NULL, "Invalid params: 'port' must be between 1 and 65535", JSONRPC_INVALID_PARAMS);
        }
        config->mqtt_broker_port = (uint16_t)port_val;
    }

    if (username_json) {
        char *new_username = strdup(username_json->valuestring);
        if (!new_username) {
            config_free(config);
            return jsonrpc_response_create(NULL, "Out of memory", JSONRPC_INTERNAL_ERROR);
        }
        free(config->mqtt_username);
        config->mqtt_username = new_username;
    }

    if (password_json) {
        char *new_password = strdup(password_json->valuestring);
        if (!new_password) {
            config_free(config);
            return jsonrpc_response_create(NULL, "Out of memory", JSONRPC_INTERNAL_ERROR);
        }
        free(config->mqtt_password);
        config->mqtt_password = new_password;
    }

    config_save(config);

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        config_free(config);
        return jsonrpc_response_create(NULL, "Out of memory while building response", JSONRPC_INTERNAL_ERROR);
    }

    if (config->mqtt_broker_host) {
        cJSON_AddStringToObject(result, "host", config->mqtt_broker_host);
    }
    cJSON_AddNumberToObject(result, "port", config->mqtt_broker_port);
    cJSON_AddStringToObject(result, "status", "reconnecting");

    config_free(config);

    mqtt_manager_stop();
    mqtt_manager_start();

    return jsonrpc_response_create(result, NULL, 0);
}

JsonRpcResponse *m_mqtt_info(cJSON *params) {
    (void)params;

    MqttStatus st = mqtt_manager_get_status();

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return jsonrpc_response_create(NULL, "Out of memory while building response", JSONRPC_INTERNAL_ERROR);
    }

    cJSON_AddBoolToObject(result, "connected", st.connected);
    cJSON_AddStringToObject(result, "host", st.host);
    cJSON_AddNumberToObject(result, "port", st.port);
    cJSON_AddStringToObject(result, "username", st.username);

    return jsonrpc_response_create(result, NULL, 0);
}

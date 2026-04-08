#include "mqtt_manager.h"

#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "rpc_m.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;
static char s_broker_host[128];
static bool s_client_started = false;

static const char *normalize_mqtt_host(const char *raw_host, uint16_t *out_port) {
  if (!raw_host || !out_port) {
    return NULL;
  }

  const char *p = raw_host;
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }

  if (strncmp(p, "mqtt://", 7) == 0) {
    p += 7;
  } else if (strncmp(p, "tcp://", 6) == 0) {
    p += 6;
  }

  while (*p == '/' || *p == ':') {
    p++;
  }

  size_t i = 0;
  while (*p && *p != '/' && !isspace((unsigned char)*p) && i < sizeof(s_broker_host) - 1) {
    s_broker_host[i++] = *p++;
  }
  s_broker_host[i] = '\0';

  if (s_broker_host[0] == '\0') {
    return NULL;
  }

  char *port_sep = strrchr(s_broker_host, ':');
  if (port_sep) {
    bool numeric_port = true;
    for (char *q = port_sep + 1; *q; ++q) {
      if (!isdigit((unsigned char)*q)) {
        numeric_port = false;
        break;
      }
    }

    if (numeric_port) {
      long parsed = strtol(port_sep + 1, NULL, 10);
      if (parsed > 0 && parsed <= 65535) {
        *out_port = (uint16_t)parsed;
        *port_sep = '\0';
      }
    }
  }

  return s_broker_host;
}

/* ------------------------------------------------------------------ */
/* Topic helpers                                                        */
/* ------------------------------------------------------------------ */

static char s_topic_rpc[64];      /* <device>/rpc        – subscribe  */
static char s_topic_rpc_resp[72]; /* <device>/rpc/response – publish  */
static char s_topic_status[64];   /* <device>/status     – heartbeat  */

/* ------------------------------------------------------------------ */
/* Heartbeat timer                                                      */
/* ------------------------------------------------------------------ */

static esp_timer_handle_t s_heartbeat_timer = NULL;

static void heartbeat_cb(void *arg) {
  if (!s_connected) {
    return;
  }

  uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
  uint32_t free_heap = esp_get_free_heap_size();

  /* Build a small JSON status payload */
  char buf[160];
  snprintf(buf, sizeof(buf), "{\"uptime\":%lu,\"free_heap\":%lu,\"device\":\"%s\"}", (unsigned long)uptime_s, (unsigned long)free_heap, device_name());

  mqtt_manager_publish(s_topic_status, buf);
  ESP_LOGD(TAG, "Heartbeat: %s", buf);
}

/* ------------------------------------------------------------------ */
/* MQTT event handler                                                   */
/* ------------------------------------------------------------------ */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

  switch ((esp_mqtt_event_id_t)event_id) {

  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "Connected to broker");
    s_connected = true;

    int sub_id = esp_mqtt_client_subscribe(s_client, s_topic_rpc, 1);
    if (sub_id < 0) {
      ESP_LOGW(TAG, "Subscription to %s failed (id=%d)", s_topic_rpc, sub_id);
    } else {
      ESP_LOGI(TAG, "Subscribed to %s", s_topic_rpc);
    }

    /* Kick off heartbeat timer */
    if (s_heartbeat_timer) {
      esp_timer_start_periodic(s_heartbeat_timer, (uint64_t)MQTT_HEARTBEAT_INTERVAL_S * 1000000ULL);
    }

    /* Publish an immediate online announcement */
    {
      char online_payload[128];
      snprintf(online_payload, sizeof(online_payload), "{\"event\":\"online\",\"device\":\"%s\"}", device_name());
      int pub_id = esp_mqtt_client_publish(s_client, s_topic_status, online_payload, 0, 1, 0);
      if (pub_id < 0) {
        ESP_LOGW(TAG, "Online announcement publish failed (id=%d)", pub_id);
      }
    }
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGW(TAG, "Disconnected from broker");
    s_connected = false;
    if (s_heartbeat_timer) {
      esp_timer_stop(s_heartbeat_timer);
    }
    break;

  case MQTT_EVENT_DATA: {
    /* Only handle messages on the RPC topic */
    if (event->topic_len == 0) {
      break; /* fragmented or unusual delivery */
    }

    /* Null-terminate topic for comparison */
    char topic_buf[128];
    size_t tlen = (event->topic_len < sizeof(topic_buf) - 1) ? (size_t)event->topic_len : sizeof(topic_buf) - 1;
    memcpy(topic_buf, event->topic, tlen);
    topic_buf[tlen] = '\0';

    if (strcmp(topic_buf, s_topic_rpc) != 0) {
      break;
    }

    /* Null-terminate payload */
    char *payload = malloc(event->data_len + 1);
    if (!payload) {
      ESP_LOGE(TAG, "OOM handling RPC message");
      break;
    }
    memcpy(payload, event->data, event->data_len);
    payload[event->data_len] = '\0';

    char *response = rpc_process_request(payload);
    free(payload);

    if (response) {
      mqtt_manager_publish(s_topic_rpc_resp, response);
      free(response);
    }
    break;
  }

  case MQTT_EVENT_ERROR:
    if (event->error_handle) {
      ESP_LOGE(TAG,
               "MQTT error: type=%d tls_last=0x%x tls_stack=0x%x "
               "transport_sock=%d cert_flags=0x%x",
               event->error_handle->error_type,
               event->error_handle->esp_tls_last_esp_err,
               event->error_handle->esp_tls_stack_err,
               event->error_handle->esp_transport_sock_errno,
               event->error_handle->esp_tls_cert_verify_flags);
    } else {
      ESP_LOGE(TAG, "MQTT error (no details)");
    }
    break;

  default:
    break;
  }
}

static esp_err_t mqtt_manager_start_internal(AppConfig *config) {
  if (s_client_started) {
    return ESP_OK;
  }

  /* Build topic strings */
  snprintf(s_topic_rpc, sizeof(s_topic_rpc), "%s/rpc", device_name());
  snprintf(s_topic_rpc_resp, sizeof(s_topic_rpc_resp), "%s/rpc/response", device_name());
  snprintf(s_topic_status, sizeof(s_topic_status), "%s/status", device_name());

  /* Create heartbeat timer (one-shot=false => periodic) */
  if (!s_heartbeat_timer) {
    esp_timer_create_args_t timer_args = {
        .callback = heartbeat_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "mqtt_hb",
    };
    esp_err_t err = esp_timer_create(&timer_args, &s_heartbeat_timer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create heartbeat timer: %s", esp_err_to_name(err));
      return err;
    }
  }

  uint16_t broker_port = config->mqtt_broker_port;
  const char *broker_host = normalize_mqtt_host(config->mqtt_broker_host, &broker_port);
  if (!broker_host) {
    ESP_LOGE(TAG, "Invalid MQTT broker host: '%s'", config->mqtt_broker_host ? config->mqtt_broker_host : "(null)");
    return ESP_ERR_INVALID_ARG;
  }

  if (strcmp(broker_host, config->mqtt_broker_host) != 0 || broker_port != config->mqtt_broker_port) {
    ESP_LOGW(TAG, "Normalized MQTT broker from '%s:%d' to '%s:%u'", config->mqtt_broker_host, config->mqtt_broker_port, broker_host, broker_port);
  }

  /* Configure and start MQTT client */
  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.hostname = broker_host,
      .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
      .broker.address.port = broker_port,
      .credentials.username = config->mqtt_username,
      .credentials.authentication.password = config->mqtt_password,
  };

  s_client = esp_mqtt_client_init(&mqtt_cfg);
  if (!s_client) {
    ESP_LOGE(TAG, "Failed to init MQTT client");
    return ESP_FAIL;
  }

  esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

  esp_err_t err = esp_mqtt_client_start(s_client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
    return err;
  }

  ESP_LOGI(TAG, "MQTT client started, broker=%s:%u", broker_host, broker_port);
  s_client_started = true;
  return ESP_OK;
}

MqttStatus mqtt_manager_get_status(void) {
  MqttStatus st = {0};
  st.connected = s_connected;
  strncpy(st.host, s_broker_host, sizeof(st.host) - 1);

  AppConfig *config = config_get();
  if (config) {
    st.port = config->mqtt_broker_port;
    if (config->mqtt_username) {
      strncpy(st.username, config->mqtt_username, sizeof(st.username) - 1);
    }
    config_free(config);
  }
  return st;
}

esp_err_t mqtt_manager_publish(const char *topic, const char *data) {
  if (!topic || !data) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!s_connected || !s_client) {
    ESP_LOGW(TAG, "MQTT publish requested but not connected (topic=%s)", topic);
    return ESP_FAIL;
  }

  int msg_id = esp_mqtt_client_publish(s_client, topic, data, 0 /* use strlen */, 1 /* QoS 1 */, 0);
  if (msg_id < 0) {
    ESP_LOGW(TAG, "MQTT publish failed (topic=%s, msg_id=%d)", topic, msg_id);
    return ESP_FAIL;
  }
  return ESP_OK;
}

void mqtt_manager_stop(void) {
  if (!s_client_started || !s_client) {
    return;
  }

  if (s_heartbeat_timer) {
    esp_timer_stop(s_heartbeat_timer);
  }

  esp_mqtt_client_stop(s_client);
  esp_mqtt_client_destroy(s_client);
  s_client = NULL;
  s_connected = false;
  s_client_started = false;
  ESP_LOGI(TAG, "MQTT manager stopped");
}

void mqtt_manager_start(void) {
  AppConfig *config = config_get();
  if (!config) {
    ESP_LOGE(TAG, "No config available");
    return;
  }

  if (s_client_started) {
    ESP_LOGI(TAG, "MQTT manager already started");
    return;
  }

  esp_err_t err = mqtt_manager_start_internal(config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start MQTT manager: %s", esp_err_to_name(err));
  }
}

#include "http_server.h"

#include "config.h"
#include "esp_log.h"
#include "rpc_m.h"
#include "web_binary.h"

#include <stdint.h>
#include <stdlib.h>

#define FILE_SIZE(start, end) (end - start)

static const char *TAG = "HTTP";

/* JSON-RPC Request Handler */
#define JSONRPC_MAX_BODY 2048

static void http_send_err(httpd_req_t *req, const char *message, int code) {
  httpd_resp_set_status(req, "400 Bad Request");

  JsonRpcResponse *response = jsonrpc_response_create(NULL, message, code);
  char *json_str = jsonrpc_response_to_json(response);
  jsonrpc_response_free(response);
  httpd_resp_sendstr(req, json_str);
  free(json_str);
}

static esp_err_t jsonrpc_handler(httpd_req_t *req) {
  char *buf = NULL;
  char *response_str = NULL;
  size_t buf_len = req->content_len;
  httpd_resp_set_type(req, "application/json");
  if (buf_len == 0) {
    http_send_err(req, "Empty request", JSONRPC_INVALID_REQUEST);
    return ESP_OK;
  }

  if (buf_len > JSONRPC_MAX_BODY) {
    http_send_err(req, "Request body too large", JSONRPC_INVALID_REQUEST);
    return ESP_OK;
  }

  buf = malloc(buf_len + 1);
  if (buf == NULL) {
    http_send_err(req, "Memory allocation failed", JSONRPC_INTERNAL_ERROR);
    return ESP_OK;
  }

  size_t received = 0;
  while (received < buf_len) {
    int ret = httpd_req_recv(req, buf + received, buf_len - received);
    if (ret <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        continue; /* Retry until content_len bytes are read */
      }
      free(buf);
      http_send_err(req, "Failed to receive request", JSONRPC_INTERNAL_ERROR);
      return ESP_FAIL;
    }
    received += (size_t)ret;
  }
  buf[received] = '\0';

  ESP_LOGI(TAG, "HTTP Received: %s", buf);

  response_str = rpc_process_request(buf);
  free(buf);

  if (response_str != NULL) {
    httpd_resp_sendstr(req, response_str);
    free(response_str);
  } else {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
  }
  return ESP_OK;
}
static httpd_uri_t jsonrpc_uri = {.uri = "/rpc", .method = HTTP_POST, .handler = jsonrpc_handler, .user_ctx = NULL};

// 1. 返回 index.html
esp_err_t http_index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html"); // MIME类型
  httpd_resp_send(req, (const char *)_binary_index_html_start, FILE_SIZE(_binary_index_html_start, _binary_index_html_end));
  return ESP_OK;
}

static httpd_uri_t ui_uri = {.uri = "/", .method = HTTP_GET, .handler = http_index_handler, .user_ctx = NULL};
static httpd_uri_t ui_uri_index = {.uri = "/index.html", .method = HTTP_GET, .handler = http_index_handler, .user_ctx = NULL};

// 2. 返回 CSS
esp_err_t http_css_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/css");
  httpd_resp_send(req, (const char *)_binary_style_css_start, FILE_SIZE(_binary_style_css_start, _binary_style_css_end));
  return ESP_OK;
}
static httpd_uri_t ui_uri_css = {.uri = "/style.css", .method = HTTP_GET, .handler = http_css_handler, .user_ctx = NULL};

// 3. 返回 JS
esp_err_t http_js_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/javascript");
  httpd_resp_send(req, (const char *)_binary_app_js_start, FILE_SIZE(_binary_app_js_start, _binary_app_js_end));
  return ESP_OK;
}
static httpd_uri_t ui_uri_js = {.uri = "/app.js", .method = HTTP_GET, .handler = http_js_handler, .user_ctx = NULL};
// 4. 返回图标
esp_err_t http_favicon_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char *)_binary_favicon_ico_start, FILE_SIZE(_binary_favicon_ico_start, _binary_favicon_ico_end));
  return ESP_OK;
}

static httpd_uri_t ui_uri_favicon = {.uri = "/favicon.ico", .method = HTTP_GET, .handler = http_favicon_handler, .user_ctx = NULL};

httpd_handle_t http_server_start(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.max_uri_handlers = 10;
  config.max_resp_headers = 8;
  config.lru_purge_enable = true;

  ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    /* Register URI handlers */
    httpd_register_uri_handler(server, &ui_uri);
    httpd_register_uri_handler(server, &ui_uri_index);
    httpd_register_uri_handler(server, &ui_uri_css);
    httpd_register_uri_handler(server, &ui_uri_js);
    httpd_register_uri_handler(server, &ui_uri_favicon);
    // json rpc
    httpd_register_uri_handler(server, &jsonrpc_uri);
    return server;
  }

  ESP_LOGI(TAG, "Error starting HTTP Server!");
  return NULL;
}

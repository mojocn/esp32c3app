#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_http_server.h"

/**
 * @brief Start the HTTP server with JSON-RPC endpoint
 * @return HTTP server handle, or NULL on failure
 */
httpd_handle_t http_server_start(void);

#endif // HTTP_SERVER_H

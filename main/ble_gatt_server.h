#ifndef BLE_GATT_SERVER_H
#define BLE_GATT_SERVER_H

#include "esp_err.h"

/**
 * @brief Initialize and start BLE GATT server exposing JSON-RPC over NUS-compatible service.
 *
 * Service:    6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * RX char:    6E400002-B5A3-F393-E0A9-E50E24DCCA9E  (Write / Write Without Response)
 *             Client writes a JSON-RPC 2.0 request to this characteristic.
 * TX char:    6E400003-B5A3-F393-E0A9-E50E24DCCA9E  (Notify)
 *             Server sends the JSON-RPC 2.0 response back via notifications.
 *             For responses larger than ATT_MTU-3, they are split into sequential
 *             chunks; the client reassembles them by concatenating until a complete
 *             JSON object is received.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t ble_gatt_server_init(void);

#endif // BLE_GATT_SERVER_H

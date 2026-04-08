# ESP32-C3 JSON-RPC Firmware

Firmware for the ESP32-C3 that exposes a **JSON-RPC 2.0** API over three transports simultaneously: HTTP, BLE (NUS-compatible GATT), and MQTT.

## Features

| Feature         | Details                                                     |
| --------------- | ----------------------------------------------------------- |
| JSON-RPC 2.0    | HTTP POST `/rpc`, BLE GATT NUS service, MQTT                |
| RGB LED         | WS2812 NeoPixel on GPIO 8                                   |
| GPIO LEDs       | GPIOs 0, 4, 5                                               |
| DHT11 sensor    | Temperature & humidity on GPIO 2                            |
| MAX7219 display | 8×8 LED matrix via SPI (CS=19, CLK=1, DIN=18)               |
| Buzzer          | Active buzzer on GPIO 10                                    |
| WiFi            | STA + AP modes, NVS-persisted config                        |
| OTA             | HTTP/HTTPS firmware update via `Sys.Ota`                    |
| Config          | NVS persistence, SPIFFS fallback (`spiffs/wifi_config.txt`) |

## Hardware Pinout

| Signal           | GPIO |
| ---------------- | ---- |
| RGB LED (WS2812) | 8    |
| DHT11 data       | 2    |
| Buzzer           | 10   |
| LED 0            | 0    |
| LED 4            | 4    |
| LED 5            | 5    |
| MAX7219 CS       | 19   |
| MAX7219 CLK      | 1    |
| MAX7219 DIN      | 18   |

## Getting Started

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/) v5.x
- Target: `esp32c3`

### Build & Flash

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Initial WiFi Configuration

On first boot, place credentials in `spiffs/wifi_config.txt` (one line: SSID, next line: password):

```
MySSID
MyPassword
```

The config is written to NVS and can be updated at runtime via `Wifi.Sta.Set`.

## JSON-RPC 2.0 API

All requests use the standard JSON-RPC 2.0 envelope:

```json
{"jsonrpc":"2.0","method":"METHOD","params":{...},"id":1}
```

### Transports

| Transport | Details                                                                                                                        |
| --------- | ------------------------------------------------------------------------------------------------------------------------------ |
| **HTTP**  | `POST http://<device-ip>/rpc` with `Content-Type: application/json`                                                            |
| **BLE**   | Write request to RX characteristic `6E400002-…`, read response via notifications on TX `6E400003-…` (NUS service `6E400001-…`) |
| **MQTT**  | Configured via `Config.Set`; device publishes heartbeats and responds to RPC topics                                            |

### Methods

#### `Sys.Info`

Returns chip information.

```json
// request
{"jsonrpc":"2.0","method":"Sys.Info","id":1}

// response
{
  "model": "esp32c3",
  "cores": 1,
  "revision": 3,
  "free_heap": 204800,
  "idf_version": "v5.2.0",
  "device_name": "esp32c3_AABBCC"
}
```

#### `Sys.Reboot`

Schedules a reboot 500 ms after the response is sent.

#### `Sys.Factory`

Erases NVS and reboots to factory defaults.

#### `Sys.Methods`

Returns a list of all registered RPC method names.

#### `Sys.Ota`

Triggers an OTA firmware update. The device reboots on success.

```json
{
  "jsonrpc": "2.0",
  "method": "Sys.Ota",
  "params": { "url": "http://192.168.1.10/firmware.bin" },
  "id": 1
}
```

#### `Wifi.Info`

Returns current WiFi status (STA IP, AP status, RSSI, etc.).

#### `Wifi.Sta.Set`

Configure station (client) mode.

```json
{
  "jsonrpc": "2.0",
  "method": "Wifi.Sta.Set",
  "params": { "enable": true, "ssid": "MyNet", "password": "s3cr3t" },
  "id": 1
}
```

#### `Wifi.Ap.Set`

Configure access point mode.

```json
{
  "jsonrpc": "2.0",
  "method": "Wifi.Ap.Set",
  "params": { "enable": true, "ssid": "esp32-ap", "password": "12345678" },
  "id": 1
}
```

#### `Ble.Info`

Returns BLE device name and connection state.

#### `Ht.Info`

Returns the latest DHT11 temperature and humidity reading.

```json
// response
{ "temperature": 24.5, "humidity": 55.0 }
```

#### `Display.Effect`

Runs a numbered animation on the MAX7219 8×8 display (1–16).

```json
{
  "jsonrpc": "2.0",
  "method": "Display.Effect",
  "params": { "effect": 3 },
  "id": 1
}
```

#### `Light.Led.Set`

Set a GPIO LED on or off.

```json
{
  "jsonrpc": "2.0",
  "method": "Light.Led.Set",
  "params": { "gpio": 4, "state": 1 },
  "id": 1
}
```

#### `Light.Rgb.Set`

Set the WS2812 RGB LED color.

```json
{
  "jsonrpc": "2.0",
  "method": "Light.Rgb.Set",
  "params": { "on": 1, "r": 255, "g": 128, "b": 0 },
  "id": 1
}
```

#### `Config.Get`

Returns the full device configuration as JSON (passwords redacted).

#### `Config.Set`

Updates device configuration. Persisted to NVS immediately.

```json
{
  "jsonrpc": "2.0",
  "method": "Config.Set",
  "params": {
    "device_name": "my-device",
    "mqtt_broker_host": "192.168.1.5",
    "mqtt_broker_port": 1883,
    "mqtt_username": "user",
    "mqtt_password": "pass"
  },
  "id": 1
}
```

## Project Structure

```
main/
  main.c              – Entry point, peripheral initialization
  config.[ch]         – NVS-backed AppConfig (WiFi, MQTT, device name)
  wifi_manager.[ch]   – WiFi STA/AP management
  http_server.[ch]    – HTTP server with /rpc endpoint
  ble_gatt_server.[ch]– BLE NUS-compatible GATT server
  mqtt_manager.[ch]   – MQTT client and heartbeat
  ota_manager.[ch]    – OTA firmware update task
  rpc_m.[ch]          – RPC dispatcher (method registry)
  rpc_json.[ch]       – JSON-RPC 2.0 envelope parsing & response helpers
  rpc_m_sys.[ch]      – Sys.* handlers
  rpc_m_wifi.[ch]     – Wifi.* handlers
  rpc_m_ble.[ch]      – Ble.* handlers
  rpc_m_ht.[ch]       – Ht.* handlers (DHT11)
  rpc_m_display.[ch]  – Display.* handlers (MAX7219)
  rpc_m_light.[ch]    – Light.* handlers (LED, RGB)
  rpc_m_config.[ch]   – Config.* handlers
  dht11.[ch]          – DHT11 driver with periodic background task
  max7219.[ch]        – MAX7219 SPI driver + effect engine
  gpio_led.[ch]       – GPIO LED driver
  gpio_rgb.[ch]       – WS2812 RGB LED driver (via led_strip component)
  buzzer.[ch]         – Buzzer driver
  app_event.[ch]      – Application-level event bus
spiffs/
  wifi_config.txt     – Default WiFi credentials (SSID / password)
partitions.csv        – Custom partition table
sdkconfig.defaults    – Default Kconfig overrides
```

## Configuration

Runtime configuration is stored in NVS under the `config` namespace and can be managed via `Config.Get` / `Config.Set`. The MQTT heartbeat interval is fixed at compile time (`MQTT_HEARTBEAT_INTERVAL_S`, default 60 s).

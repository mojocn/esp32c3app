/* BLE GATT server – JSON-RPC 2.0 over NUS-compatible service
 *
 * Uses the ESP-IDF NimBLE stack (IDF 5.x).
 * Requires CONFIG_BT_ENABLED=y and CONFIG_BT_NIMBLE_ENABLED=y in sdkconfig.
 */

#include "ble_gatt_server.h"

#include "config.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "rpc_m.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "BLE_GATT";

/* ---- NUS Service / Characteristic UUIDs (128-bit, little-endian) ----------
 * Service:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * RX char:  6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 * TX char:  6E400003-B5A3-F393-E0A9-E50E24DCCA9E
 */
static const ble_uuid128_t nus_svc_uuid = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t nus_rx_uuid = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t nus_tx_uuid = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

/* ---- Runtime state -------------------------------------------------------- */
static uint16_t tx_chr_handle = 0;
static uint16_t ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool tx_notify_enabled = false;

/* ---- Forward declarations ------------------------------------------------- */
static void ble_app_advertise(void);
static int nus_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

/* ---- GATT service table --------------------------------------------------- */
static const struct ble_gatt_svc_def nus_svc_def[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &nus_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    /* TX characteristic – server → client (Notify) */
                    .uuid = &nus_tx_uuid.u,
                    .access_cb = nus_access_cb,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &tx_chr_handle,
                },
                {
                    /* RX characteristic – client → server (Write / Write No Response) */
                    .uuid = &nus_rx_uuid.u,
                    .access_cb = nus_access_cb,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                },
                {0},
            },
    },
    {0},
};

/* ---- Notification helper (with ATT-MTU-aware chunking) -------------------- */
static void nus_notify(const char *data, size_t total_len) {
  if (!tx_notify_enabled || ble_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    return;
  }

  uint16_t mtu = ble_att_mtu(ble_conn_handle);
  uint16_t chunk_max = (mtu > 3) ? (uint16_t)(mtu - 3) : 20;

  for (size_t offset = 0; offset < total_len; offset += chunk_max) {
    size_t chunk = (total_len - offset > chunk_max) ? chunk_max : (total_len - offset);

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data + offset, chunk);
    if (!om) {
      ESP_LOGE(TAG, "mbuf alloc failed at offset %d", (int)offset);
      break;
    }

    int rc = ble_gatts_notify_custom(ble_conn_handle, tx_chr_handle, om);
    if (rc != 0) {
      ESP_LOGE(TAG, "notify error %d at offset %d", rc, (int)offset);
      break;
    }
  }
}

/* ---- GATT access callback ------------------------------------------------- */
static int nus_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
    return 0;
  }

  uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);
  char *request = malloc(data_len + 1);
  if (!request) {
    ESP_LOGE(TAG, "OOM allocating RPC request buffer (%u bytes)", data_len);
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  uint16_t read_len = 0;
  int rc = ble_hs_mbuf_to_flat(ctxt->om, request, data_len, &read_len);
  if (rc != 0) {
    free(request);
    return BLE_ATT_ERR_UNLIKELY;
  }
  request[read_len] = '\0';

  ESP_LOGI(TAG, "BLE RPC <<< %s", request);

  char *response = rpc_process_request(request);
  free(request);

  if (response) {
    ESP_LOGI(TAG, "BLE RPC >>> %s", response);
    nus_notify(response, strlen(response));
    free(response);
  }

  return 0;
}

/* ---- GAP event handler ---------------------------------------------------- */
static int ble_gap_event_cb(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0) {
      ble_conn_handle = event->connect.conn_handle;
      tx_notify_enabled = false;
      ESP_LOGI(TAG, "BLE connected (handle=%d)", ble_conn_handle);
      /* Request larger MTU to accommodate bigger JSON responses */
      ble_att_set_preferred_mtu(512);
      ble_gattc_exchange_mtu(ble_conn_handle, NULL, NULL);
    } else {
      ESP_LOGW(TAG, "BLE connection failed (status=%d), restarting adv", event->connect.status);
      ble_app_advertise();
    }
    break;

  case BLE_GAP_EVENT_DISCONNECT:
    ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    tx_notify_enabled = false;
    ESP_LOGI(TAG, "BLE disconnected (reason=%d)", event->disconnect.reason);
    ble_app_advertise();
    break;

  case BLE_GAP_EVENT_SUBSCRIBE:
    if (event->subscribe.attr_handle == tx_chr_handle) {
      tx_notify_enabled = (event->subscribe.cur_notify != 0);
      ESP_LOGI(TAG, "BLE TX notifications %s", tx_notify_enabled ? "enabled" : "disabled");
    }
    break;

  case BLE_GAP_EVENT_MTU:
    ESP_LOGI(TAG, "BLE MTU updated: conn=%d, mtu=%d", event->mtu.conn_handle, event->mtu.value);
    break;

  default:
    break;
  }
  return 0;
}

/* ---- Advertising ---------------------------------------------------------- */
static void ble_app_advertise(void) {
  struct ble_hs_adv_fields fields = {0};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  const char *name = device_name();
  fields.name = (const uint8_t *)name;
  fields.name_len = (uint8_t)strlen(name);
  fields.name_is_complete = 1;

  int rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gap_adv_set_fields error: %d", rc);
    return;
  }

  struct ble_gap_adv_params adv_params = {0};
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event_cb, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gap_adv_start error: %d", rc);
  } else {
    ESP_LOGI(TAG, "BLE advertising as \"%s\"", name);
  }
}

/* ---- NimBLE host callbacks ------------------------------------------------ */
static void ble_app_on_sync(void) {
  int rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_hs_util_ensure_addr error: %d", rc);
    return;
  }
  ble_app_advertise();
}

static void ble_host_task(void *param) {
  ESP_LOGI(TAG, "BLE host task started");
  nimble_port_run(); /* Blocks until nimble_port_stop() */
  nimble_port_freertos_deinit();
}

/* ---- Public API ----------------------------------------------------------- */
esp_err_t ble_gatt_server_init(void) {
  esp_err_t err = nimble_port_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
    return err;
  }

  /* NimBLE host configuration */
  ble_hs_cfg.sync_cb = ble_app_on_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ble_hs_cfg.sm_bonding = 0;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;

  /* Standard GAP and GATT services */
  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_svc_gap_device_name_set(device_name());

  /* Register application GATT service */
  int rc = ble_gatts_count_cfg(nus_svc_def);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gatts_count_cfg error: %d", rc);
    return ESP_FAIL;
  }

  rc = ble_gatts_add_svcs(nus_svc_def);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gatts_add_svcs error: %d", rc);
    return ESP_FAIL;
  }

  nimble_port_freertos_init(ble_host_task);
  ESP_LOGI(TAG, "BLE GATT server initialized (NUS-compatible JSON-RPC service)");
  return ESP_OK;
}

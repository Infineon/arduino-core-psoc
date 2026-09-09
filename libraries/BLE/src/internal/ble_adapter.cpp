/*
 * Internal adapter implementation. This is the ONLY translation unit in the
 * BLE library allowed to include btstack/btstack-integration headers and
 * call wiced_bt_* / cybt_* APIs directly (see ble_adapter.h for rationale).
 */

#include "internal/ble_adapter.h"

extern "C" {
#include "cybt_platform_config.h"
#include "cybsp_bt_config.h"
#include "wiced_bt_stack.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_gatt.h"
}

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

namespace ble_internal {

    namespace {

/* Maximum raw advertising elements this adapter builds: flags + one
 * advertised service UUID. The local name is placed in the scan response
 * so a 128-bit service UUID cannot overflow legacy advertising's 31-byte
 * payload limit. */
        constexpr uint8_t BLE_ADAPTER_MAX_ADVERT_ELEMENTS = 2;

/* Parses a hex nibble; returns -1 for non-hex characters. */
        int hex_nibble(char c) {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f') {
                return 10 + (c - 'a');
            }
            if (c >= 'A' && c <= 'F') {
                return 10 + (c - 'A');
            }
            return -1;
        }

/* Parses a UUID string into its little-endian over-the-air byte
 * representation. Accepts a 16-bit UUID ("180D") or a dashed 128-bit UUID
 * ("19b10000-e8f2-537e-4f6c-d104768a1214"). On success, fills 'out' (which
 * must be at least 16 bytes) and sets 'out_len' to 2 or 16. Returns false on
 * a malformed UUID string. */
        bool parse_uuid(const char *uuid, uint8_t *out, uint8_t &out_len) {
            if (uuid == nullptr) {
                return false;
            }

            /* Collect hex nibbles, skipping dashes, in the order they appear
             * in the (big-endian, human-readable) string. */
            uint8_t big_endian_bytes[16];
            int byte_count = 0;
            int nibble_high = -1;

            for (const char *p = uuid; *p != '\0'; p++) {
                if (*p == '-') {
                    continue;
                }
                int nibble = hex_nibble(*p);
                if (nibble < 0) {
                    return false;
                }
                if (nibble_high < 0) {
                    nibble_high = nibble;
                } else {
                    if (byte_count >= 16) {
                        return false; /* Too long to be a 16-bit or 128-bit UUID. */
                    }
                    big_endian_bytes[byte_count++] = (uint8_t)((nibble_high << 4) | nibble);
                    nibble_high = -1;
                }
            }

            if (nibble_high >= 0) {
                return false; /* Odd number of hex digits. */
            }

            if (byte_count == 2) {
                /* 16-bit UUID: over-the-air order is little-endian. */
                out[0] = big_endian_bytes[1];
                out[1] = big_endian_bytes[0];
                out_len = 2;
                return true;
            }

            if (byte_count == 16) {
                /* 128-bit UUID: over-the-air order is the reverse of the
                 * textual (big-endian) representation. */
                for (int i = 0; i < 16; i++) {
                    out[i] = big_endian_bytes[15 - i];
                }
                out_len = 16;
                return true;
            }

            return false;
        }

/* Formats a device address (6 raw bytes, over-the-air/wiced order) as
 * "AA:BB:CC:DD:EE:FF" into 'out' (must be at least 18 bytes). */
        void format_address(const uint8_t *addr, char *out, size_t out_size) {
            snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X",
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        }

/* Parses a "AA:BB:CC:DD:EE:FF" address string (as produced by
 * format_address(), e.g. from BLEDevice::address()) into 6 raw bytes.
 * Returns false if 'address' isn't exactly that format. */
        bool parse_address(const char *address, uint8_t *out) {
            if (address == nullptr) {
                return false;
            }
            unsigned int bytes[6];
            if (sscanf(address, "%02x:%02x:%02x:%02x:%02x:%02x",
                &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]) != 6) {
                return false;
            }
            for (int i = 0; i < 6; i++) {
                out[i] = (uint8_t)bytes[i];
            }
            return true;
        }

/* Formats a UUID's over-the-air bytes (as returned by
 * wiced_bt_ble_check_advertising_data(), little-endian for 16-bit, reversed
 * textual order for 128-bit - see parse_uuid() above for the inverse
 * mapping) back into a lowercase human-readable UUID string. 'byte_len'
 * must be 2 or 16; 'out' must be at least 37 bytes. Returns false for any
 * other byte_len. */
        bool format_uuid_bytes(const uint8_t *air_bytes, uint8_t byte_len, char *out, size_t out_size) {
            if (byte_len == 2) {
                snprintf(out, out_size, "%02x%02x", air_bytes[1], air_bytes[0]);
                return true;
            }

            if (byte_len == 16) {
                uint8_t big_endian_bytes[16];
                for (int i = 0; i < 16; i++) {
                    big_endian_bytes[i] = air_bytes[15 - i];
                }
                snprintf(out, out_size,
                    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                    big_endian_bytes[0], big_endian_bytes[1], big_endian_bytes[2], big_endian_bytes[3],
                    big_endian_bytes[4], big_endian_bytes[5],
                    big_endian_bytes[6], big_endian_bytes[7],
                    big_endian_bytes[8], big_endian_bytes[9],
                    big_endian_bytes[10], big_endian_bytes[11], big_endian_bytes[12],
                    big_endian_bytes[13], big_endian_bytes[14], big_endian_bytes[15]);
                return true;
            }

            return false;
        }

/* Formats a wiced_bt_gatt discovery UUID (as returned in discovery results
 * for services/characteristics) into a lowercase human-readable UUID
 * string, using the same over-the-air byte convention as
 * format_uuid_bytes() above (wiced_bt_uuid_t.uu.uuid128 is stored in that
 * same wire order). 'out' must be at least 37 bytes. */
        bool format_wiced_uuid(const wiced_bt_uuid_t &uuid, char *out, size_t out_size) {
            if (uuid.len == LEN_UUID_16) {
                uint8_t air_bytes[2] = { (uint8_t)(uuid.uu.uuid16 & 0xFF), (uint8_t)((uuid.uu.uuid16 >> 8) & 0xFF) };
                return format_uuid_bytes(air_bytes, 2, out, out_size);
            }
            if (uuid.len == LEN_UUID_128) {
                return format_uuid_bytes(uuid.uu.uuid128, 16, out, out_size);
            }
            if (out_size > 0) {
                out[0] = '\0';
            }
            return false;
        }

/* Cursor-based byte appenders used by gatt_db_append_service()/
 * gatt_db_append_characteristic() below to build the raw GATT database
 * byte array passed to wiced_bt_gatt_db_init(), matching the byte layout
 * the vendor SDK's PRIMARY_SERVICE_UUID16/128 and
 * CHARACTERISTIC_UUID16/128_WRITABLE macros (wiced_bt_gatt.h) would
 * produce for a static initializer - reproduced here byte-for-byte since
 * this library builds the database at runtime (from sketch-registered
 * BLEService/BLECharacteristic objects) rather than at compile time. */
        void gatt_db_write_u8(uint8_t *buf, int &idx, uint8_t v) {
            buf[idx++] = v;
        }

        void gatt_db_write_u16(uint8_t *buf, int &idx, uint16_t v) {
            buf[idx++] = (uint8_t)(v & 0xFF);
            buf[idx++] = (uint8_t)((v >> 8) & 0xFF);
        }

        void gatt_db_write_bytes(uint8_t *buf, int &idx, const uint8_t *data, uint8_t len) {
            memcpy(buf + idx, data, len);
            idx += len;
        }

/* Appends a PRIMARY_SERVICE_UUID16/128 declaration attribute. */
        void gatt_db_append_service(uint8_t *buf, int &idx, uint16_t handle,
            const uint8_t *uuid, uint8_t uuid_len) {
            gatt_db_write_u16(buf, idx, handle);
            gatt_db_write_u8(buf, idx, GATTDB_PERM_READABLE);
            gatt_db_write_u8(buf, idx, (uint8_t)(2 + uuid_len));
            gatt_db_write_u16(buf, idx, GATT_UUID_PRI_SERVICE);
            gatt_db_write_bytes(buf, idx, uuid, uuid_len);
        }

/* Appends a CHARACTERISTIC_UUID16_WRITABLE/128_WRITABLE declaration +
 * value attribute pair. The value attribute's own bytes are an unused
 * placeholder (as in the vendor macros): this library always serves reads/
 * writes for its own characteristics via GATT_ATTRIBUTE_REQUEST_EVT (see
 * BLEAdapter::on_gatt_attribute_request()), looking the current value up
 * from the corresponding BLECharacteristic by handle rather than from the
 * database bytes themselves. */
        void gatt_db_append_characteristic(uint8_t *buf, int &idx, uint16_t decl_handle, uint16_t value_handle,
            const uint8_t *uuid, uint8_t uuid_len, uint8_t properties, uint8_t permission) {
            gatt_db_write_u16(buf, idx, decl_handle);
            gatt_db_write_u8(buf, idx, GATTDB_PERM_READABLE);
            gatt_db_write_u8(buf, idx, (uint8_t)(2 + 1 + 2 + uuid_len));
            gatt_db_write_u16(buf, idx, GATT_UUID_CHAR_DECLARE);
            gatt_db_write_u8(buf, idx, properties);
            gatt_db_write_u16(buf, idx, value_handle);
            gatt_db_write_bytes(buf, idx, uuid, uuid_len);

            uint8_t value_permission = permission;
            if (uuid_len == GATTDB_UUID128_SIZE) {
                value_permission |= GATTDB_PERM_SERVICE_UUID_128;
            }
            gatt_db_write_u16(buf, idx, value_handle);
            gatt_db_write_u8(buf, idx, value_permission);
            gatt_db_write_u8(buf, idx, uuid_len);
            if (uuid_len == GATTDB_UUID128_SIZE) {
                /* The 128-bit vendor macro includes a reserved byte in the
                 * value record; the 16-bit form does not. */
                gatt_db_write_u8(buf, idx, 0);
            }
            gatt_db_write_bytes(buf, idx, uuid, uuid_len);
        }

/* Appends a CHAR_DESCRIPTOR_UUID16_WRITABLE declaration for the Client
 * Characteristic Configuration Descriptor (used to track subscribe state
 * for BLENotify/BLEIndicate characteristics; the value itself, like
 * characteristic values above, is served via GATT_ATTRIBUTE_REQUEST_EVT
 * rather than the database bytes - see
 * BLEAdapter::on_gatt_attribute_request()'s find_local_characteristic_by_cccd()
 * handling). Uses GATTDB_PERM_WRITE_REQ/_WRITE_CMD directly rather than the
 * GATTDB_PERM_WRITABLE macro, which also sets GATTDB_PERM_AUTH_WRITABLE -
 * that bit makes the stack's built-in permission check silently reject the
 * write before it ever reaches the app callback, since this library never
 * pairs/encrypts the link (security_required = 0, see PRD). */
        void gatt_db_append_cccd(uint8_t *buf, int &idx, uint16_t handle) {
            gatt_db_write_u16(buf, idx, handle);
            gatt_db_write_u8(buf, idx, GATTDB_PERM_READABLE | GATTDB_PERM_WRITE_REQ | GATTDB_PERM_WRITE_CMD);
            gatt_db_write_u8(buf, idx, GATTDB_UUID16_SIZE);
            gatt_db_write_u8(buf, idx, 0);
            gatt_db_write_u16(buf, idx, GATT_UUID_CHAR_CLIENT_CONFIG);
        }

/* Scans 'adv_data' for every occurrence of the 16-/128-bit service UUID list
 * advertising data type 'ad_type' (which may pack multiple 'uuid_byte_len'
 * -sized UUIDs back to back) and appends each as a formatted string to
 * 'result', bounded by BLE_ADAPTER_MAX_SCAN_SERVICE_UUIDS. */
        void append_service_uuids(const uint8_t *adv_data, wiced_bt_ble_advert_type_t ad_type,
            uint8_t uuid_byte_len, ble_adapter_scan_result_t &result) {
            uint8_t length = 0;
            uint8_t *data = wiced_bt_ble_check_advertising_data(const_cast < uint8_t * > (adv_data), ad_type, &length);
            if (data == nullptr) {
                return;
            }

            for (uint8_t offset = 0; offset + uuid_byte_len <= length
                 && result.service_uuid_count < BLE_ADAPTER_MAX_SCAN_SERVICE_UUIDS;
                 offset += uuid_byte_len) {
                format_uuid_bytes(data + offset, uuid_byte_len,
                    result.service_uuids[result.service_uuid_count],
                    sizeof(result.service_uuids[result.service_uuid_count]));
                result.service_uuid_count++;
            }
        }

/* Bounds how long init()/deinit() block waiting for the corresponding
 * BTM_ENABLED_EVT/BTM_DISABLED_EVT to arrive from the bt_task. */
        constexpr TickType_t BLE_ADAPTER_LIFECYCLE_TIMEOUT_TICKS = pdMS_TO_TICKS(5000);
        constexpr UBaseType_t BLE_ADAPTER_EVENT_QUEUE_LENGTH = 16;

/* Bounds how long connect()/disconnect() block waiting for the
 * corresponding GATT_CONNECTION_STATUS_EVT to arrive from the bt_task.
 * Connection establishment involves an over-the-air exchange with the
 * peer, so this is generously longer than the lifecycle timeout above. */
        constexpr TickType_t BLE_ADAPTER_CONNECTION_TIMEOUT_TICKS = pdMS_TO_TICKS(10000);

/* Minimal, hand-authored LE-only stack configuration (single peripheral/
 * central connection, no bonding/pairing - see PRD "Connection topology"
 * decision). Later slices may extend GATT/advertising specific fields but
 * should not need to touch the lifecycle-critical fields below. */
        const wiced_bt_cfg_ble_scan_settings_t ble_scan_cfg = {
            /* Active scanning is required (not passive) so the controller
             * sends SCAN_REQ and captures SCAN_RSP packets - many peripherals
             * (including ArduinoBLE, via BLELocalDevice::setLocalName())
             * place the advertised local name in the scan response rather
             * than the primary advertising data, so passive scanning would
             * never observe it. */
            .scan_mode = BTM_BLE_SCAN_MODE_ACTIVE,
            .high_duty_scan_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_INTERVAL,
            .high_duty_scan_window = WICED_BT_CFG_DEFAULT_HIGH_DUTY_SCAN_WINDOW,
            .high_duty_scan_duration = 5,
            .low_duty_scan_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_SCAN_INTERVAL,
            .low_duty_scan_window = WICED_BT_CFG_DEFAULT_LOW_DUTY_SCAN_WINDOW,
            .low_duty_scan_duration = 0,
            .high_duty_conn_scan_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_CONN_SCAN_INTERVAL,
            .high_duty_conn_scan_window = WICED_BT_CFG_DEFAULT_HIGH_DUTY_CONN_SCAN_WINDOW,
            .high_duty_conn_duration = 30,
            .low_duty_conn_scan_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_CONN_SCAN_INTERVAL,
            .low_duty_conn_scan_window = WICED_BT_CFG_DEFAULT_LOW_DUTY_CONN_SCAN_WINDOW,
            .low_duty_conn_duration = 0,
            .conn_min_interval = WICED_BT_CFG_DEFAULT_CONN_MIN_INTERVAL,
            .conn_max_interval = WICED_BT_CFG_DEFAULT_CONN_MAX_INTERVAL,
            .conn_latency = 0,
            .conn_supervision_timeout = WICED_BT_CFG_DEFAULT_CONN_SUPERVISION_TIMEOUT,
        };

        const wiced_bt_cfg_ble_advert_settings_t ble_advert_cfg = {
            .channel_map = (BTM_BLE_ADVERT_CHNL_37 | BTM_BLE_ADVERT_CHNL_38 | BTM_BLE_ADVERT_CHNL_39),
            .high_duty_min_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MIN_INTERVAL,
            .high_duty_max_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MAX_INTERVAL,
            .high_duty_duration = 30,
            .low_duty_min_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MIN_INTERVAL,
            .low_duty_max_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MAX_INTERVAL,
            .low_duty_duration = 0,
            .high_duty_directed_min_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MIN_INTERVAL,
            .high_duty_directed_max_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MAX_INTERVAL,
            .low_duty_directed_min_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MIN_INTERVAL,
            .low_duty_directed_max_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MAX_INTERVAL,
            .low_duty_directed_duration = 0,
            .high_duty_nonconn_min_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MIN_INTERVAL,
            .high_duty_nonconn_max_interval = WICED_BT_CFG_DEFAULT_HIGH_DUTY_ADV_MAX_INTERVAL,
            .high_duty_nonconn_duration = 0,
            .low_duty_nonconn_min_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MIN_INTERVAL,
            .low_duty_nonconn_max_interval = WICED_BT_CFG_DEFAULT_LOW_DUTY_ADV_MAX_INTERVAL,
            .low_duty_nonconn_duration = 0,
        };

        const wiced_bt_cfg_ble_t ble_cfg = {
            .ble_max_simultaneous_links = 1, /* single active connection per role, see PRD */
            .ble_max_rx_pdu_size = 251,
            .appearance = APPEARANCE_GENERIC_TAG,
            .rpa_refresh_timeout = 0,  /* LE privacy disabled */
            .host_addr_resolution_db_size = 0,
            .p_ble_scan_cfg = &ble_scan_cfg,
            .p_ble_advert_cfg = &ble_advert_cfg,
            .default_ble_power_level = 0,
        };

        const wiced_bt_cfg_gatt_t gatt_cfg = {
            .max_db_service_modules = 0,
            .max_eatt_bearers = 0,
        };

        const wiced_bt_cfg_br_t br_cfg = {}; /* BLE-only: BR/EDR configuration left at zero. */
        const wiced_bt_cfg_isoc_t isoc_cfg = {};
        const wiced_bt_cfg_l2cap_application_t l2cap_app_cfg = {};

/* Device name buffer backing wiced_bt_cfg_settings_t.device_name (must
 * outlive the call to wiced_bt_stack_init). */
        uint8_t g_device_name[32] = "PSOC6-BLE";

        wiced_bt_cfg_settings_t g_bt_cfg_settings = {
            .device_name = g_device_name,
            .security_required = 0, /* no bits set: no pairing/bonding/encryption required, see PRD */
            .p_br_cfg = &br_cfg,
            .p_ble_cfg = &ble_cfg,
            .p_gatt_cfg = &gatt_cfg,
            .p_isoc_cfg = &isoc_cfg,
            .p_l2cap_app_cfg = &l2cap_app_cfg,
        };

    } // namespace

/*
 * Registered directly with wiced_bt_stack_init(). This plain function (not
 * a class member) is the only code that runs on the BLESS-IPC bt_task
 * context; it forwards to BLEAdapter's public on_stack_*() handlers, which
 * only push a lightweight event into the thread-safe queue and signal the
 * lifecycle semaphore. No other application state is touched here.
 */
    static wiced_result_t ble_adapter_management_callback(wiced_bt_management_evt_t event,
        wiced_bt_management_evt_data_t *p_event_data) {
        switch (event) {
            case BTM_ENABLED_EVT:
                BLEAdapter::instance().on_stack_enabled();
                break;
            case BTM_DISABLED_EVT:
                BLEAdapter::instance().on_stack_disabled();
                break;
            default:
                /* Unhandled events are ignored for this lifecycle-only slice;
                 * later slices (advertising/connections/GATT) add cases here. */
                break;
        }
        return WICED_BT_SUCCESS;
    }

/*
 * Registered directly with wiced_bt_ble_scan(). Runs on the BLESS-IPC
 * bt_task context for every discovered advertising/scan-response packet;
 * forwards to BLEAdapter's public on_scan_result(), which only builds a
 * fixed-size event and pushes it into the thread-safe queue. No other
 * application state is touched here.
 */
    static void ble_adapter_scan_result_callback(wiced_bt_ble_scan_results_t *p_scan_result,
        uint8_t *p_adv_data) {
        BLEAdapter::instance().on_scan_result(p_scan_result, p_adv_data);
    }

/*
 * Registered directly with wiced_bt_gatt_register(). Runs on the
 * BLESS-IPC bt_task context for every GATT event; forwards each to the
 * corresponding BLEAdapter handler, which only updates internal state,
 * pushes fixed-size events into the thread-safe queue, and/or signals the
 * relevant semaphore. No other application state is touched here.
 */
    static wiced_bt_gatt_status_t ble_adapter_gatt_callback(wiced_bt_gatt_evt_t event,
        wiced_bt_gatt_event_data_t *p_event_data) {
        switch (event) {
            case GATT_CONNECTION_STATUS_EVT:
                BLEAdapter::instance().on_gatt_connection_status(&p_event_data->connection_status);
                break;
            case GATT_ATTRIBUTE_REQUEST_EVT:
                BLEAdapter::instance().on_gatt_attribute_request(
                    p_event_data->attribute_request.conn_id, &p_event_data->attribute_request);
                break;
            case GATT_DISCOVERY_RESULT_EVT:
                BLEAdapter::instance().on_gatt_discovery_result(&p_event_data->discovery_result);
                break;
            case GATT_DISCOVERY_CPLT_EVT:
                BLEAdapter::instance().on_gatt_discovery_complete(&p_event_data->discovery_complete);
                break;
            case GATT_OPERATION_CPLT_EVT:
                BLEAdapter::instance().on_gatt_operation_complete(&p_event_data->operation_complete);
                break;
            default:
                /* Other event types (congestion, buffer lifecycle) have
                 * nothing to do here yet. */
                break;
        }
        return WICED_BT_GATT_SUCCESS;
    }

    BLEAdapter::BLEAdapter()
        : _initialized(false),
        _stack_init_started(false),
        _last_error(BLE_ADAPTER_ERROR_NONE),
        _pending_scan_result_valid(false),
        _event_queue(nullptr),
        _lifecycle_sem(nullptr),
        _connected(false),
        _is_local_central(false),
        _conn_id(0),
        _connection_sem(nullptr),
        _gatt_registered(false),
        _gatt_db_registered(false),
        _local_characteristic_count(0),
        _discovered_service_count(0),
        _discovery_current_service_index(-1),
        _discovery_sem(nullptr),
        _discovery_status(WICED_BT_GATT_SUCCESS),
        _gatt_op_sem(nullptr),
        _gatt_op_status(WICED_BT_GATT_SUCCESS),
        _gatt_op_result_len(0) {
        _scan_service_uuid_filter[0] = '\0';
        _peer_address[0] = '\0';
        _connecting_address[0] = '\0';
        for (int i = 0; i < MAX_LOCAL_CHARACTERISTICS; i++) {
            _local_characteristics[i] = nullptr;
        }
        for (int i = 0; i < MAX_DISCOVERED_SERVICES; i++) {
            _discovered_services[i] = nullptr;
        }
    }

    BLEAdapter::~BLEAdapter() {
        free_discovered_services();
        if (_event_queue != nullptr) {
            vQueueDelete(_event_queue);
        }
        if (_lifecycle_sem != nullptr) {
            vSemaphoreDelete(_lifecycle_sem);
        }
        if (_connection_sem != nullptr) {
            vSemaphoreDelete(_connection_sem);
        }
        if (_discovery_sem != nullptr) {
            vSemaphoreDelete(_discovery_sem);
        }
        if (_gatt_op_sem != nullptr) {
            vSemaphoreDelete(_gatt_op_sem);
        }
    }

    BLEAdapter & BLEAdapter::instance() {
        static BLEAdapter adapter;
        return adapter;
    }

    bool BLEAdapter::init(const char *device_name) {
        if (_initialized) {
            xQueueReset(_event_queue);
            _last_error = BLE_ADAPTER_ERROR_NONE;
            return true;
        }

        if (device_name != nullptr) {
            strncpy((char *)g_device_name, device_name, sizeof(g_device_name) - 1);
            g_device_name[sizeof(g_device_name) - 1] = '\0';
        }

        if (_event_queue == nullptr) {
            _event_queue = xQueueCreate(BLE_ADAPTER_EVENT_QUEUE_LENGTH, sizeof(ble_adapter_event_t));
            if (_event_queue == nullptr) {
                _last_error = BLE_ADAPTER_ERROR_QUEUE_CREATE_FAILED;
                return false;
            }
        } else {
            xQueueReset(_event_queue);
        }

        if (_lifecycle_sem == nullptr) {
            _lifecycle_sem = xSemaphoreCreateBinary();
            if (_lifecycle_sem == nullptr) {
                _last_error = BLE_ADAPTER_ERROR_SEMAPHORE_CREATE_FAILED;
                return false;
            }
        }

        if (_connection_sem == nullptr) {
            _connection_sem = xSemaphoreCreateBinary();
            if (_connection_sem == nullptr) {
                _last_error = BLE_ADAPTER_ERROR_SEMAPHORE_CREATE_FAILED;
                return false;
            }
        }

        if (_discovery_sem == nullptr) {
            _discovery_sem = xSemaphoreCreateBinary();
            if (_discovery_sem == nullptr) {
                _last_error = BLE_ADAPTER_ERROR_SEMAPHORE_CREATE_FAILED;
                return false;
            }
        }

        if (_gatt_op_sem == nullptr) {
            _gatt_op_sem = xSemaphoreCreateBinary();
            if (_gatt_op_sem == nullptr) {
                _last_error = BLE_ADAPTER_ERROR_SEMAPHORE_CREATE_FAILED;
                return false;
            }
        }

        if (_stack_init_started) {
            if (xSemaphoreTake(_lifecycle_sem, BLE_ADAPTER_LIFECYCLE_TIMEOUT_TICKS) != pdTRUE) {
                _last_error = BLE_ADAPTER_ERROR_STACK_INIT_TIMEOUT;
                return false;
            }

            _initialized = true;
            if (!_gatt_registered && wiced_bt_gatt_register(ble_adapter_gatt_callback) == WICED_BT_GATT_SUCCESS) {
                _gatt_registered = true;
            }
            _last_error = BLE_ADAPTER_ERROR_NONE;
            return true;
        }

        /* Drain any stale signal from a previous lifecycle transition before
         * requesting a fresh stack start. */
        xSemaphoreTake(_lifecycle_sem, 0);

        cybt_platform_config_init(&cybsp_bt_platform_cfg);

        wiced_result_t result = wiced_bt_stack_init(ble_adapter_management_callback, &g_bt_cfg_settings);
        if (result != WICED_BT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_STACK_INIT_FAILED;
            return false;
        }

        _stack_init_started = true;

        if (xSemaphoreTake(_lifecycle_sem, BLE_ADAPTER_LIFECYCLE_TIMEOUT_TICKS) != pdTRUE) {
            _last_error = BLE_ADAPTER_ERROR_STACK_INIT_TIMEOUT;
            return false;
        }

        _initialized = true;
        if (!_gatt_registered && wiced_bt_gatt_register(ble_adapter_gatt_callback) == WICED_BT_GATT_SUCCESS) {
            _gatt_registered = true;
        }
        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    bool BLEAdapter::deinit() {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }

        xQueueReset(_event_queue);
        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    bool BLEAdapter::pop_event(ble_adapter_event_t &event) {
        if (!_initialized || _event_queue == nullptr) {
            return false;
        }
        return xQueueReceive(_event_queue, &event, 0) == pdTRUE;
    }

    void BLEAdapter::push_event(ble_adapter_event_type_t type) {
        if (_event_queue == nullptr) {
            return;
        }
        ble_adapter_event_t event;
        event.type = type;
        /* Called from the bt_task context; never blocks the caller. If the
         * queue is momentarily full the oldest, not-yet-drained event is
         * dropped rather than stalling the BT stack task. */
        if (xQueueSend(_event_queue, &event, 0) != pdTRUE) {
            ble_adapter_event_t dropped;
            xQueueReceive(_event_queue, &dropped, 0);
            xQueueSend(_event_queue, &event, 0);
        }
    }

    void BLEAdapter::push_scan_result_event(const ble_adapter_scan_result_t &scan_result) {
        if (_event_queue == nullptr) {
            return;
        }
        ble_adapter_event_t event;
        event.type = BLE_ADAPTER_EVENT_SCAN_RESULT;
        event.scan_result = scan_result;
        /* Called from the bt_task context; never blocks the caller. If the
         * queue is momentarily full the oldest, not-yet-drained event is
         * dropped rather than stalling the BT stack task. */
        if (xQueueSend(_event_queue, &event, 0) != pdTRUE) {
            ble_adapter_event_t dropped;
            xQueueReceive(_event_queue, &dropped, 0);
            xQueueSend(_event_queue, &event, 0);
        }
    }

    void BLEAdapter::push_connection_event(ble_adapter_event_type_t type, const char *address, bool is_local_central) {
        if (_event_queue == nullptr) {
            return;
        }
        ble_adapter_event_t event;
        event.type = type;
        strncpy(event.connection.address, address, sizeof(event.connection.address) - 1);
        event.connection.address[sizeof(event.connection.address) - 1] = '\0';
        event.connection.is_local_central = is_local_central;
        /* Called from the bt_task context; never blocks the caller. If the
         * queue is momentarily full the oldest, not-yet-drained event is
         * dropped rather than stalling the BT stack task. */
        if (xQueueSend(_event_queue, &event, 0) != pdTRUE) {
            ble_adapter_event_t dropped;
            xQueueReceive(_event_queue, &dropped, 0);
            xQueueSend(_event_queue, &event, 0);
        }
    }

    void BLEAdapter::maybe_queue_scan_result(const ble_adapter_scan_result_t &scan_result) {
        if (_scan_service_uuid_filter[0] != '\0') {
            bool matches_filter = false;
            for (uint8_t i = 0; i < scan_result.service_uuid_count; i++) {
                if (strcasecmp(scan_result.service_uuids[i], _scan_service_uuid_filter) == 0) {
                    matches_filter = true;
                    break;
                }
            }
            if (!matches_filter) {
                return;
            }
        }

        push_scan_result_event(scan_result);
    }

    void BLEAdapter::on_scan_result(const void *p_scan_result, const uint8_t *p_adv_data) {
        /* NULL signals the end of a bounded scan; scanning here is otherwise
         * continuous until stop_scan(), so there is nothing to do. */
        if (p_scan_result == nullptr) {
            return;
        }

        const wiced_bt_ble_scan_results_t *result =
            reinterpret_cast < const wiced_bt_ble_scan_results_t * > (p_scan_result);

        ble_adapter_scan_result_t scan_result;
        format_address(result->remote_bd_addr, scan_result.address, sizeof(scan_result.address));
        scan_result.rssi = result->rssi;
        scan_result.local_name[0] = '\0';
        scan_result.service_uuid_count = 0;

        if (p_adv_data != nullptr) {
            uint8_t name_len = 0;
            uint8_t *name = wiced_bt_ble_check_advertising_data(const_cast < uint8_t * > (p_adv_data),
                BTM_BLE_ADVERT_TYPE_NAME_COMPLETE, &name_len);
            if (name == nullptr) {
                name = wiced_bt_ble_check_advertising_data(const_cast < uint8_t * > (p_adv_data),
                    BTM_BLE_ADVERT_TYPE_NAME_SHORT, &name_len);
            }
            if (name != nullptr && name_len > 0) {
                uint8_t copy_len = (name_len < sizeof(scan_result.local_name) - 1)
                    ? name_len : (uint8_t)(sizeof(scan_result.local_name) - 1);
                memcpy(scan_result.local_name, name, copy_len);
                scan_result.local_name[copy_len] = '\0';
            }

            append_service_uuids(p_adv_data, BTM_BLE_ADVERT_TYPE_16SRV_COMPLETE, 2, scan_result);
            append_service_uuids(p_adv_data, BTM_BLE_ADVERT_TYPE_16SRV_PARTIAL, 2, scan_result);
            append_service_uuids(p_adv_data, BTM_BLE_ADVERT_TYPE_128SRV_COMPLETE, 16, scan_result);
            append_service_uuids(p_adv_data, BTM_BLE_ADVERT_TYPE_128SRV_PARTIAL, 16, scan_result);
        }

        /* With active scanning, a scannable peripheral's local name (e.g.
         * ArduinoBLE's BLE.setLocalName(), which places it in the scan
         * response - see BLELocalDevice::setLocalName()) typically arrives
         * in a separate SCAN_RSP report from the ADV_IND report carrying
         * flags/service UUIDs. Merge the two by address into a single
         * BLEDevice rather than surfacing two incomplete records. */
        if (result->ble_evt_type == BTM_BLE_EVT_SCAN_RSP) {
            if (_pending_scan_result_valid &&
                strcasecmp(_pending_scan_result.address, scan_result.address) == 0) {
                /* Fill in whatever the pending ADV_IND report didn't have. */
                if (_pending_scan_result.local_name[0] == '\0' && scan_result.local_name[0] != '\0') {
                    strncpy(_pending_scan_result.local_name, scan_result.local_name,
                        sizeof(_pending_scan_result.local_name) - 1);
                    _pending_scan_result.local_name[sizeof(_pending_scan_result.local_name) - 1] = '\0';
                }
                for (uint8_t i = 0; i < scan_result.service_uuid_count &&
                     _pending_scan_result.service_uuid_count < BLE_ADAPTER_MAX_SCAN_SERVICE_UUIDS; i++) {
                    bool already_present = false;
                    for (uint8_t j = 0; j < _pending_scan_result.service_uuid_count; j++) {
                        if (strcasecmp(_pending_scan_result.service_uuids[j], scan_result.service_uuids[i]) == 0) {
                            already_present = true;
                            break;
                        }
                    }
                    if (!already_present) {
                        strncpy(_pending_scan_result.service_uuids[_pending_scan_result.service_uuid_count],
                            scan_result.service_uuids[i], sizeof(_pending_scan_result.service_uuids[0]) - 1);
                        _pending_scan_result.service_uuid_count++;
                    }
                }
                maybe_queue_scan_result(_pending_scan_result);
                _pending_scan_result_valid = false;
                return;
            }

            /* A SCAN_RSP with no matching pending ADV_IND (e.g. it arrived
             * out of order, or the ADV_IND wasn't scannable per its event
             * type) - surface what this report alone contains rather than
             * dropping it. */
            maybe_queue_scan_result(scan_result);
            return;
        }

        bool is_scannable = (result->ble_evt_type == BTM_BLE_EVT_CONNECTABLE_ADVERTISEMENT) ||
            (result->ble_evt_type == BTM_BLE_EVT_CONNECTABLE_DIRECTED_ADVERTISEMENT) ||
            (result->ble_evt_type == BTM_BLE_EVT_SCANNABLE_ADVERTISEMENT);

        if (is_scannable) {
            /* A previous pending entry that never got its matching SCAN_RSP
             * (e.g. the peripheral didn't respond) would otherwise be lost
             * silently - surface it now, before it's overwritten. */
            if (_pending_scan_result_valid) {
                maybe_queue_scan_result(_pending_scan_result);
            }
            _pending_scan_result = scan_result;
            _pending_scan_result_valid = true;
            return;
        }

        /* Non-connectable advertisement: no scan response will ever
         * follow, so queue immediately. */
        maybe_queue_scan_result(scan_result);
    }


    bool BLEAdapter::start_scan(const char *service_uuid_filter) {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }

        if (service_uuid_filter != nullptr && service_uuid_filter[0] != '\0') {
            strncpy(_scan_service_uuid_filter, service_uuid_filter, sizeof(_scan_service_uuid_filter) - 1);
            _scan_service_uuid_filter[sizeof(_scan_service_uuid_filter) - 1] = '\0';
        } else {
            _scan_service_uuid_filter[0] = '\0';
        }

        _pending_scan_result_valid = false;

        /* wiced_bt_ble_scan() returns WICED_BT_PENDING (not WICED_BT_SUCCESS)
         * when the scan request has been successfully queued to the
         * controller - only actual failure codes indicate a real error. */
        wiced_result_t result = wiced_bt_ble_scan(BTM_BLE_SCAN_TYPE_LOW_DUTY, WICED_TRUE, ble_adapter_scan_result_callback);
        if (result != WICED_BT_SUCCESS && result != WICED_BT_PENDING) {
            _last_error = BLE_ADAPTER_ERROR_SCAN_START_FAILED;
            return false;
        }

        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    bool BLEAdapter::stop_scan() {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }

        wiced_result_t result = wiced_bt_ble_scan(BTM_BLE_SCAN_TYPE_NONE, WICED_FALSE, nullptr);
        if (result != WICED_BT_SUCCESS && result != WICED_BT_PENDING) {
            _last_error = BLE_ADAPTER_ERROR_SCAN_STOP_FAILED;
            return false;
        }

        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    bool BLEAdapter::connect(const char *address) {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }

        if (_connected) {
            _last_error = BLE_ADAPTER_ERROR_ALREADY_CONNECTED;
            return false;
        }

        uint8_t bd_addr[6];
        if (!parse_address(address, bd_addr)) {
            _last_error = BLE_ADAPTER_ERROR_INVALID_ADDRESS;
            return false;
        }

        strncpy(_connecting_address, address, sizeof(_connecting_address) - 1);
        _connecting_address[sizeof(_connecting_address) - 1] = '\0';
        _is_local_central = true;

        /* Drain any stale signal from a previous connect()/disconnect()
         * before requesting a fresh connection. */
        xSemaphoreTake(_connection_sem, 0);

        if (!wiced_bt_gatt_le_connect(bd_addr, BLE_ADDR_PUBLIC, BLE_CONN_MODE_HIGH_DUTY, WICED_TRUE)) {
            _last_error = BLE_ADAPTER_ERROR_CONNECT_FAILED;
            return false;
        }

        if (xSemaphoreTake(_connection_sem, BLE_ADAPTER_CONNECTION_TIMEOUT_TICKS) != pdTRUE) {
            wiced_bt_gatt_cancel_connect(bd_addr, WICED_TRUE);
            _last_error = BLE_ADAPTER_ERROR_CONNECT_TIMEOUT;
            return false;
        }

        if (!_connected || strcasecmp(_peer_address, address) != 0) {
            /* Woke up for a connection-status event, but not the successful
             * connection to 'address' being waited for (e.g. the remote
             * rejected/timed out the request). */
            _last_error = BLE_ADAPTER_ERROR_CONNECT_FAILED;
            return false;
        }

        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    bool BLEAdapter::disconnect() {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }

        if (!_connected) {
            _last_error = BLE_ADAPTER_ERROR_NONE;
            return true;
        }

        /* Drain any stale signal from a previous connect()/disconnect()
         * before requesting the disconnect. */
        xSemaphoreTake(_connection_sem, 0);

        if (wiced_bt_gatt_disconnect(_conn_id) != WICED_BT_GATT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_DISCONNECT_FAILED;
            return false;
        }

        if (xSemaphoreTake(_connection_sem, BLE_ADAPTER_CONNECTION_TIMEOUT_TICKS) != pdTRUE) {
            _last_error = BLE_ADAPTER_ERROR_DISCONNECT_TIMEOUT;
            return false;
        }

        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    void BLEAdapter::on_gatt_connection_status(const void *p_connection_status) {
        const wiced_bt_gatt_connection_status_t *status =
            reinterpret_cast < const wiced_bt_gatt_connection_status_t * > (p_connection_status);

        char address[18];
        format_address(status->bd_addr, address, sizeof(address));

        if (status->connected) {
            _connected = true;
            _conn_id = status->conn_id;
            strncpy(_peer_address, address, sizeof(_peer_address) - 1);
            _peer_address[sizeof(_peer_address) - 1] = '\0';
            /* If this connection completed a pending connect() to this same
             * address, it's local-central; otherwise it's an unsolicited
             * incoming connection (remote-initiated, i.e. this device is
             * acting as peripheral). */
            _is_local_central = (_connecting_address[0] != '\0'
                && strcasecmp(_connecting_address, address) == 0);
            push_connection_event(BLE_ADAPTER_EVENT_CONNECTED, address, _is_local_central);
        } else {
            _connected = false;
            push_connection_event(BLE_ADAPTER_EVENT_DISCONNECTED, address, _is_local_central);
            _peer_address[0] = '\0';
            _is_local_central = false;
            /* Discovered attribute handles are only valid for the
             * connection they were discovered on. */
            free_discovered_services();
        }

        _connecting_address[0] = '\0';

        if (_connection_sem != nullptr) {
            xSemaphoreGive(_connection_sem);
        }
    }

    void BLEAdapter::on_stack_enabled() {
        _initialized = true;
        push_event(BLE_ADAPTER_EVENT_STACK_ENABLED);
        if (_lifecycle_sem != nullptr) {
            xSemaphoreGive(_lifecycle_sem);
        }
    }

    bool BLEAdapter::start_advertising(const ble_adapter_advert_params_t &params) {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }

        wiced_bt_ble_advert_elem_t elems[BLE_ADAPTER_MAX_ADVERT_ELEMENTS];
        uint8_t elem_count = 0;

        /* Flags: general discoverable, LE-only (no BR/EDR). */
        static uint8_t flags = BTM_BLE_GENERAL_DISCOVERABLE_FLAG | BTM_BLE_BREDR_NOT_SUPPORTED;
        elems[elem_count].advert_type = BTM_BLE_ADVERT_TYPE_FLAG;
        elems[elem_count].len = sizeof(flags);
        elems[elem_count].p_data = &flags;
        elem_count++;

        uint8_t service_uuid_bytes[16];
        uint8_t service_uuid_len = 0;
        bool have_service_uuid = params.service_uuid != nullptr && params.service_uuid[0] != '\0';
        if (have_service_uuid) {
            if (!parse_uuid(params.service_uuid, service_uuid_bytes, service_uuid_len)) {
                _last_error = BLE_ADAPTER_ERROR_INVALID_UUID;
                return false;
            }
            elems[elem_count].advert_type = (service_uuid_len == 16)
                ? BTM_BLE_ADVERT_TYPE_128SRV_COMPLETE
                : BTM_BLE_ADVERT_TYPE_16SRV_COMPLETE;
            elems[elem_count].len = service_uuid_len;
            elems[elem_count].p_data = service_uuid_bytes;
            elem_count++;
        }

        if (wiced_bt_ble_set_raw_advertisement_data(elem_count, elems) != WICED_BT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_ADVERTISE_DATA_FAILED;
            return false;
        }

        wiced_bt_ble_advert_elem_t scan_response_elem;
        uint8_t scan_response_count = 0;
        size_t name_len = params.local_name == nullptr ? 0 : strlen(params.local_name);
        if (name_len > 0) {
            constexpr size_t MAX_SCAN_RESPONSE_NAME_LENGTH = 29;
            bool name_is_complete = name_len <= MAX_SCAN_RESPONSE_NAME_LENGTH;
            if (!name_is_complete) {
                name_len = MAX_SCAN_RESPONSE_NAME_LENGTH;
            }
            scan_response_elem.advert_type = name_is_complete
                ? BTM_BLE_ADVERT_TYPE_NAME_COMPLETE
                : BTM_BLE_ADVERT_TYPE_NAME_SHORT;
            scan_response_elem.len = (uint16_t)name_len;
            scan_response_elem.p_data = (uint8_t *)params.local_name;
            scan_response_count = 1;
        }
        if (wiced_bt_ble_set_raw_scan_response_data(scan_response_count,
            scan_response_count == 0 ? nullptr : &scan_response_elem) != WICED_BT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_ADVERTISE_DATA_FAILED;
            return false;
        }

        if (wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, nullptr) != WICED_BT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_ADVERTISE_START_FAILED;
            return false;
        }

        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    bool BLEAdapter::stop_advertising() {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }

        if (wiced_bt_start_advertisements(BTM_BLE_ADVERT_OFF, 0, nullptr) != WICED_BT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_ADVERTISE_START_FAILED;
            return false;
        }

        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    void BLEAdapter::on_stack_disabled() {
        _initialized = false;
        _stack_init_started = false;
        push_event(BLE_ADAPTER_EVENT_STACK_DISABLED);
        if (_lifecycle_sem != nullptr) {
            xSemaphoreGive(_lifecycle_sem);
        }
    }

    bool BLEAdapter::register_gatt_database(BLEService *const *services, int service_count) {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }

        if (_gatt_db_registered) {
            _last_error = BLE_ADAPTER_ERROR_NONE;
            return true;
        }

        /* Generous per-entry byte bounds (see gatt_db_append_service()/
         * gatt_db_append_characteristic()/gatt_db_append_cccd() above), sized
         * for the worst case (128-bit UUIDs) so the exact arithmetic of each
         * attribute's byte layout doesn't need to be re-derived here. */
        constexpr int SERVICE_ENTRY_MAX_BYTES = 32;
        constexpr int CHARACTERISTIC_ENTRY_MAX_BYTES = 64;
        constexpr int CCCD_ENTRY_MAX_BYTES = 16;

        int idx = 0;
        uint16_t next_handle = 1;
        _local_characteristic_count = 0;

        for (int s = 0; s < service_count; s++) {
            BLEService *service = services[s];
            if (service == nullptr) {
                continue;
            }

            uint8_t service_uuid[16];
            uint8_t service_uuid_len = 0;
            if (!parse_uuid(service->uuid(), service_uuid, service_uuid_len)) {
                _last_error = BLE_ADAPTER_ERROR_INVALID_UUID;
                return false;
            }
            if (idx + SERVICE_ENTRY_MAX_BYTES > GATT_DB_BUFFER_SIZE) {
                _last_error = BLE_ADAPTER_ERROR_GATT_DB_REGISTER_FAILED;
                return false;
            }
            gatt_db_append_service(_gatt_db_buffer, idx, next_handle++, service_uuid, service_uuid_len);

            for (int c = 0; c < service->characteristicCount(); c++) {
                BLECharacteristic *characteristic = service->characteristic(c);
                if (characteristic == nullptr) {
                    continue;
                }

                uint8_t char_uuid[16];
                uint8_t char_uuid_len = 0;
                if (!parse_uuid(characteristic->uuid(), char_uuid, char_uuid_len)) {
                    _last_error = BLE_ADAPTER_ERROR_INVALID_UUID;
                    return false;
                }
                if (idx + CHARACTERISTIC_ENTRY_MAX_BYTES > GATT_DB_BUFFER_SIZE
                    || _local_characteristic_count >= MAX_LOCAL_CHARACTERISTICS) {
                    _last_error = BLE_ADAPTER_ERROR_GATT_DB_REGISTER_FAILED;
                    return false;
                }

                uint8_t properties = characteristic->properties();
                uint8_t permission = GATTDB_PERM_NONE;
                if (properties & (BLERead | BLENotify | BLEIndicate)) {
                    permission |= GATTDB_PERM_READABLE;
                }
                if (properties & BLEWrite) {
                    permission |= GATTDB_PERM_WRITE_REQ;
                }
                if (properties & BLEWriteWithoutResponse) {
                    permission |= GATTDB_PERM_WRITE_CMD;
                }

                uint16_t decl_handle = next_handle++;
                uint16_t value_handle = next_handle++;
                gatt_db_append_characteristic(_gatt_db_buffer, idx, decl_handle, value_handle,
                    char_uuid, char_uuid_len, properties, permission);
                characteristic->_setValueHandle(value_handle);
                characteristic->_setRemote(false);
                _local_characteristics[_local_characteristic_count++] = characteristic;

                if (properties & (BLENotify | BLEIndicate)) {
                    if (idx + CCCD_ENTRY_MAX_BYTES > GATT_DB_BUFFER_SIZE) {
                        _last_error = BLE_ADAPTER_ERROR_GATT_DB_REGISTER_FAILED;
                        return false;
                    }
                    uint16_t cccd_handle = next_handle++;
                    gatt_db_append_cccd(_gatt_db_buffer, idx, cccd_handle);
                    characteristic->_setCccdHandle(cccd_handle);
                }
            }
        }

        if (wiced_bt_gatt_db_init(_gatt_db_buffer, (uint16_t)idx, nullptr) != WICED_BT_GATT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_GATT_DB_REGISTER_FAILED;
            return false;
        }

        _gatt_db_registered = true;
        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    BLECharacteristic * BLEAdapter::find_local_characteristic(uint16_t value_handle) const {
        for (int i = 0; i < _local_characteristic_count; i++) {
            if (_local_characteristics[i] != nullptr && _local_characteristics[i]->_valueHandle() == value_handle) {
                return _local_characteristics[i];
            }
        }
        return nullptr;
    }

    void BLEAdapter::on_gatt_attribute_request(uint16_t conn_id, const void *p_attribute_request) {
        const wiced_bt_gatt_attribute_request_t *request =
            reinterpret_cast < const wiced_bt_gatt_attribute_request_t * > (p_attribute_request);

        switch (request->opcode) {
            case GATT_REQ_READ:
            case GATT_REQ_READ_BLOB: {
                uint16_t handle = request->data.read_req.handle;
                BLECharacteristic *characteristic = find_local_characteristic(handle);
                if (characteristic == nullptr) {
                    wiced_bt_gatt_server_send_error_rsp(conn_id, request->opcode, handle, WICED_BT_GATT_INVALID_HANDLE);
                    return;
                }
                uint16_t offset = request->data.read_req.offset;
                int valueLength = characteristic->valueLength();
                if (offset > valueLength) {
                    wiced_bt_gatt_server_send_error_rsp(conn_id, request->opcode, handle, WICED_BT_GATT_INVALID_OFFSET);
                    return;
                }
                wiced_bt_gatt_server_send_read_handle_rsp(conn_id, request->opcode,
                    (uint16_t)(valueLength - offset),
                    const_cast < uint8_t * > (characteristic->value()) + offset, nullptr);
                break;
            }
            case GATT_REQ_WRITE:
            case GATT_CMD_WRITE: {
                uint16_t handle = request->data.write_req.handle;
                BLECharacteristic *characteristic = find_local_characteristic(handle);
                if (characteristic == nullptr) {
                    if (request->opcode == GATT_REQ_WRITE) {
                        wiced_bt_gatt_server_send_error_rsp(conn_id, request->opcode, handle, WICED_BT_GATT_INVALID_HANDLE);
                    }
                    return;
                }
                characteristic->_setValueFromPeer(request->data.write_req.p_val, request->data.write_req.val_len);
                if (request->opcode == GATT_REQ_WRITE) {
                    wiced_bt_gatt_server_send_write_rsp(conn_id, request->opcode, handle);
                }
                break;
            }
            case GATT_REQ_MTU:
                /* Central-initiated ATT MTU exchange, sent automatically by
                 * most GATT clients (including ArduinoBLE) right after
                 * connecting, before any subsequent GATT operation (e.g. a
                 * subscribe() CCCD write). This library keeps the fixed
                 * default MTU (see PRD/BLECharacteristic's 20-byte default
                 * value size, sized for GATT_BLE_DEFAULT_MTU_SIZE - 3 ATT
                 * header bytes), but must still answer the request -
                 * leaving it unhandled (as the catch-all default below
                 * does) causes some peers (e.g. ArduinoBLE) to treat the
                 * connection as unusable and disconnect. */
                wiced_bt_gatt_server_send_mtu_rsp(conn_id, request->data.remote_mtu, GATT_BLE_DEFAULT_MTU_SIZE);
                break;
            default:
                /* Execute-write/etc are not used by this library (no
                 * reliable writes - see PRD); politely decline anything
                 * else the peer requests. */
                wiced_bt_gatt_server_send_error_rsp(conn_id, request->opcode, 0, WICED_BT_GATT_REQ_NOT_SUPPORTED);
                break;
        }
    }

    bool BLEAdapter::discover_attributes() {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }
        if (!_connected) {
            _last_error = BLE_ADAPTER_ERROR_NOT_CONNECTED;
            return false;
        }

        free_discovered_services();

        wiced_bt_gatt_discovery_param_t service_param = {};
        service_param.s_handle = 1;
        service_param.e_handle = 0xFFFF;
        _discovery_current_service_index = -1;
        if (!discover_blocking(GATT_DISCOVER_SERVICES_ALL, &service_param)) {
            return false;
        }

        /* Discover the characteristics of each service found above. A
         * failure discovering one service's characteristics doesn't abort
         * the whole pass - services/characteristics discovered so far
         * remain queryable via service()/characteristic(). */
        bool all_ok = true;
        for (int i = 0; i < _discovered_service_count; i++) {
            BLEService *service = _discovered_services[i];
            wiced_bt_gatt_discovery_param_t char_param = {};
            char_param.s_handle = service->_startHandle();
            char_param.e_handle = service->_endHandle();
            _discovery_current_service_index = i;
            if (!discover_blocking(GATT_DISCOVER_CHARACTERISTICS, &char_param)) {
                all_ok = false;
            }
        }
        _discovery_current_service_index = -1;

        _last_error = all_ok ? BLE_ADAPTER_ERROR_NONE : BLE_ADAPTER_ERROR_DISCOVERY_FAILED;
        return all_ok;
    }

    bool BLEAdapter::discover_blocking(uint8_t type, void *p_param) {
        wiced_bt_gatt_discovery_param_t *param = reinterpret_cast < wiced_bt_gatt_discovery_param_t * > (p_param);

        /* Drain any stale signal from a previous discovery step. */
        xSemaphoreTake(_discovery_sem, 0);
        _discovery_status = WICED_BT_GATT_SUCCESS;

        if (wiced_bt_gatt_client_send_discover(_conn_id, (wiced_bt_gatt_discovery_type_t)type, param)
            != WICED_BT_GATT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_DISCOVERY_FAILED;
            return false;
        }

        if (xSemaphoreTake(_discovery_sem, BLE_ADAPTER_CONNECTION_TIMEOUT_TICKS) != pdTRUE) {
            _last_error = BLE_ADAPTER_ERROR_DISCOVERY_TIMEOUT;
            return false;
        }

        if (_discovery_status != WICED_BT_GATT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_DISCOVERY_FAILED;
            return false;
        }

        return true;
    }

    void BLEAdapter::on_gatt_discovery_result(const void *p_discovery_result) {
        const wiced_bt_gatt_discovery_result_t *result =
            reinterpret_cast < const wiced_bt_gatt_discovery_result_t * > (p_discovery_result);

        if (result->discovery_type == GATT_DISCOVER_SERVICES_ALL) {
            if (_discovered_service_count >= MAX_DISCOVERED_SERVICES) {
                return;
            }
            char uuid[37];
            format_wiced_uuid(result->discovery_data.group_value.service_type, uuid, sizeof(uuid));
            BLEService *service = new BLEService(uuid);
            service->_setHandleRange(result->discovery_data.group_value.s_handle,
                result->discovery_data.group_value.e_handle);
            _discovered_services[_discovered_service_count++] = service;
        } else if (result->discovery_type == GATT_DISCOVER_CHARACTERISTICS) {
            if (_discovery_current_service_index < 0
                || _discovery_current_service_index >= _discovered_service_count) {
                return;
            }
            BLEService *service = _discovered_services[_discovery_current_service_index];
            if (service->characteristicCount() >= BLEService::MAX_CHARACTERISTICS) {
                return;
            }
            char uuid[37];
            format_wiced_uuid(result->discovery_data.characteristic_declaration.char_uuid, uuid, sizeof(uuid));
            BLECharacteristic *characteristic = new BLECharacteristic(uuid,
                result->discovery_data.characteristic_declaration.characteristic_properties);
            characteristic->_setValueHandle(result->discovery_data.characteristic_declaration.val_handle);
            characteristic->_setRemote(true);
            service->addCharacteristic(*characteristic);
        }
        /* GATT_DISCOVER_INCLUDED_SERVICES/GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS
         * results are ignored: this library doesn't discover included
         * services, and descriptor discovery (needed for subscribe/notify)
         * is issues/006-notifications-subscriptions.md's scope. */
    }

    void BLEAdapter::on_gatt_discovery_complete(const void *p_discovery_complete) {
        const wiced_bt_gatt_discovery_complete_t *complete =
            reinterpret_cast < const wiced_bt_gatt_discovery_complete_t * > (p_discovery_complete);
        _discovery_status = complete->status;
        if (_discovery_sem != nullptr) {
            xSemaphoreGive(_discovery_sem);
        }
    }

    void BLEAdapter::on_gatt_operation_complete(const void *p_operation_complete) {
        const wiced_bt_gatt_operation_complete_t *complete =
            reinterpret_cast < const wiced_bt_gatt_operation_complete_t * > (p_operation_complete);

        if (complete->op != GATTC_OPTYPE_READ_HANDLE && complete->op != GATTC_OPTYPE_WRITE_WITH_RSP
            && complete->op != GATTC_OPTYPE_WRITE_NO_RSP) {
            /* Discovery/config/notification completions are handled
             * elsewhere (on_gatt_discovery_complete()) or ignored. */
            return;
        }

        _gatt_op_status = complete->status;
        _gatt_op_result_len = complete->response_data.att_value.len;
        if (_gatt_op_sem != nullptr) {
            xSemaphoreGive(_gatt_op_sem);
        }
    }

    bool BLEAdapter::read_remote_characteristic(uint16_t value_handle, uint8_t *buffer, int buffer_length,
        int &out_length) {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }
        if (!_connected) {
            _last_error = BLE_ADAPTER_ERROR_NOT_CONNECTED;
            return false;
        }
        if (buffer == nullptr || buffer_length <= 0) {
            _last_error = BLE_ADAPTER_ERROR_READ_FAILED;
            return false;
        }

        /* Drain any stale signal from a previous GATT client operation. */
        xSemaphoreTake(_gatt_op_sem, 0);
        _gatt_op_status = WICED_BT_GATT_SUCCESS;
        _gatt_op_result_len = 0;

        if (wiced_bt_gatt_client_send_read_handle(_conn_id, value_handle, 0, buffer, (uint16_t)buffer_length,
            GATT_AUTH_REQ_NONE) != WICED_BT_GATT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_READ_FAILED;
            return false;
        }

        if (xSemaphoreTake(_gatt_op_sem, BLE_ADAPTER_CONNECTION_TIMEOUT_TICKS) != pdTRUE) {
            _last_error = BLE_ADAPTER_ERROR_READ_TIMEOUT;
            return false;
        }

        if (_gatt_op_status != WICED_BT_GATT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_READ_FAILED;
            return false;
        }

        out_length = _gatt_op_result_len;
        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    bool BLEAdapter::write_remote_characteristic(uint16_t value_handle, const uint8_t *value, int length) {
        if (!_initialized) {
            _last_error = BLE_ADAPTER_ERROR_NOT_INITIALIZED;
            return false;
        }
        if (!_connected) {
            _last_error = BLE_ADAPTER_ERROR_NOT_CONNECTED;
            return false;
        }
        if (length < 0) {
            _last_error = BLE_ADAPTER_ERROR_WRITE_FAILED;
            return false;
        }

        /* wiced_bt_gatt_client_send_write() requires the write buffer to
         * remain valid until the operation completes; since this call
         * blocks until then, a stack-local copy (bounded by the configured
         * max RX PDU size - see ble_cfg.ble_max_rx_pdu_size above) is safe. */
        uint8_t local_buffer[251];
        if ((size_t)length > sizeof(local_buffer)) {
            _last_error = BLE_ADAPTER_ERROR_WRITE_FAILED;
            return false;
        }
        if (length > 0 && value != nullptr) {
            memcpy(local_buffer, value, length);
        }

        wiced_bt_gatt_write_hdr_t hdr;
        hdr.handle = value_handle;
        hdr.offset = 0;
        hdr.len = (uint16_t)length;
        hdr.auth_req = GATT_AUTH_REQ_NONE;

        /* Drain any stale signal from a previous GATT client operation. */
        xSemaphoreTake(_gatt_op_sem, 0);
        _gatt_op_status = WICED_BT_GATT_SUCCESS;

        if (wiced_bt_gatt_client_send_write(_conn_id, GATT_REQ_WRITE, &hdr, local_buffer, nullptr)
            != WICED_BT_GATT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_WRITE_FAILED;
            return false;
        }

        if (xSemaphoreTake(_gatt_op_sem, BLE_ADAPTER_CONNECTION_TIMEOUT_TICKS) != pdTRUE) {
            _last_error = BLE_ADAPTER_ERROR_WRITE_TIMEOUT;
            return false;
        }

        if (_gatt_op_status != WICED_BT_GATT_SUCCESS) {
            _last_error = BLE_ADAPTER_ERROR_WRITE_FAILED;
            return false;
        }

        _last_error = BLE_ADAPTER_ERROR_NONE;
        return true;
    }

    int BLEAdapter::discovered_service_count() const {
        return _discovered_service_count;
    }

    BLEService * BLEAdapter::discovered_service(int index) const {
        if (index < 0 || index >= _discovered_service_count) {
            return nullptr;
        }
        return _discovered_services[index];
    }

    BLEService * BLEAdapter::find_discovered_service(const char *uuid) const {
        if (uuid == nullptr) {
            return nullptr;
        }
        for (int i = 0; i < _discovered_service_count; i++) {
            if (strcasecmp(_discovered_services[i]->uuid(), uuid) == 0) {
                return _discovered_services[i];
            }
        }
        return nullptr;
    }

    void BLEAdapter::free_discovered_services() {
        for (int i = 0; i < _discovered_service_count; i++) {
            BLEService *service = _discovered_services[i];
            if (service == nullptr) {
                continue;
            }
            for (int c = 0; c < service->characteristicCount(); c++) {
                delete service->characteristic(c);
            }
            delete service;
            _discovered_services[i] = nullptr;
        }
        _discovered_service_count = 0;
        _discovery_current_service_index = -1;
    }

} // namespace ble_internal

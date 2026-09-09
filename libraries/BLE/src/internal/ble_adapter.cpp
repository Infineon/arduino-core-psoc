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
}

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

namespace ble_internal {

    namespace {

/* Maximum raw advertising elements this adapter builds: flags + local name +
 * one advertised service UUID. */
        constexpr uint8_t BLE_ADAPTER_MAX_ADVERT_ELEMENTS = 3;

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

    BLEAdapter::BLEAdapter()
        : _initialized(false),
        _stack_init_started(false),
        _last_error(BLE_ADAPTER_ERROR_NONE),
        _pending_scan_result_valid(false),
        _event_queue(nullptr),
        _lifecycle_sem(nullptr) {
        _scan_service_uuid_filter[0] = '\0';
    }

    BLEAdapter::~BLEAdapter() {
        if (_event_queue != nullptr) {
            vQueueDelete(_event_queue);
        }
        if (_lifecycle_sem != nullptr) {
            vSemaphoreDelete(_lifecycle_sem);
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

        if (_stack_init_started) {
            if (xSemaphoreTake(_lifecycle_sem, BLE_ADAPTER_LIFECYCLE_TIMEOUT_TICKS) != pdTRUE) {
                _last_error = BLE_ADAPTER_ERROR_STACK_INIT_TIMEOUT;
                return false;
            }

            _initialized = true;
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

        size_t name_len = 0;
        if (params.local_name != nullptr) {
            name_len = strlen(params.local_name);
        }
        if (name_len > 0) {
            elems[elem_count].advert_type = BTM_BLE_ADVERT_TYPE_NAME_COMPLETE;
            elems[elem_count].len = (uint16_t)name_len;
            elems[elem_count].p_data = (uint8_t *)params.local_name;
            elem_count++;
        }

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

} // namespace ble_internal

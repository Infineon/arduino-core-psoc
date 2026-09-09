#ifndef BLE_INTERNAL_BLE_ADAPTER_H
#define BLE_INTERNAL_BLE_ADAPTER_H

/*
 * Internal adapter that owns ALL direct interaction with the MTB BT Stack
 * (btstack + btstack-integration, running over the BLESS-IPC transport).
 *
 * No public BLE class is allowed to call btstack/wiced_bt_* APIs directly:
 * everything must go through this adapter. This keeps the public facade
 * (BLE.h/.cpp and friends) free of vendor-stack details and gives later
 * slices (GATT server/client, advertising, scanning, connections, ...) a
 * single place to add btstack calls.
 *
 * Threading model:
 *  - btstack/BLESS-IPC management and GATT callbacks fire on the BLESS-IPC
 *    "bt_task" FreeRTOS task context.
 *  - The sketch (Arduino) runs on a separate "arduino-main-task".
 *  - Callbacks MUST NOT touch application state directly. They only push a
 *    small, fixed-size event struct into a FreeRTOS queue (which is
 *    internally thread-safe/ISR-safe). BLEAdapter::pop_event(), called from
 *    BLE.poll() on the sketch task, is the sole consumer draining this
 *    queue. This queue+lock pattern is the foundation all later event
 *    delivery (advertising state, connections, GATT reads/writes,
 *    notifications, ...) builds on.
 */

#include <stdbool.h>
#include <stdint.h>

extern "C" {
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
}

namespace ble_internal {

/* Last-error codes surfaced by the adapter. The public BLE facade maps
 * these 1:1 onto its own ble_error_t so sketch authors never need to know
 * about btstack/wiced result codes. */
    typedef enum {
        BLE_ADAPTER_ERROR_NONE = 0,
        BLE_ADAPTER_ERROR_ALREADY_INITIALIZED,
        BLE_ADAPTER_ERROR_NOT_INITIALIZED,
        BLE_ADAPTER_ERROR_STACK_INIT_FAILED,
        BLE_ADAPTER_ERROR_STACK_INIT_TIMEOUT,
        BLE_ADAPTER_ERROR_STACK_DEINIT_FAILED,
        BLE_ADAPTER_ERROR_STACK_DEINIT_TIMEOUT,
        BLE_ADAPTER_ERROR_QUEUE_CREATE_FAILED,
        BLE_ADAPTER_ERROR_SEMAPHORE_CREATE_FAILED,
        BLE_ADAPTER_ERROR_INVALID_UUID,
        BLE_ADAPTER_ERROR_ADVERTISE_DATA_FAILED,
        BLE_ADAPTER_ERROR_ADVERTISE_START_FAILED,
        BLE_ADAPTER_ERROR_SCAN_START_FAILED,
        BLE_ADAPTER_ERROR_SCAN_STOP_FAILED,
    } ble_adapter_error_t;

/* Maximum number of advertised service UUIDs captured per scan result
 * event (mirrors BLEDevice::MAX_ADVERTISED_SERVICE_UUIDS). */
    constexpr int BLE_ADAPTER_MAX_SCAN_SERVICE_UUIDS = 4;

/* Parameters for start_advertising(). local_name/service_uuid may be
 * nullptr or empty to omit that field from the advertising payload.
 * service_uuid accepts a 16-bit UUID string (e.g. "180D") or a 128-bit
 * UUID string (e.g. "19b10000-e8f2-537e-4f6c-d104768a1214"). */
    typedef struct {
        const char *local_name;
        const char *service_uuid;
    } ble_adapter_advert_params_t;

/* One discovered peripheral's data, captured from its advertising/scan
 * response packet(s) at the moment it was seen. Kept as fixed-size buffers
 * (no heap/pointers into transient btstack buffers) so it can be copied by
 * value into the event queue below. */
    typedef struct {
        char address[18]; /* "AA:BB:CC:DD:EE:FF\0" */
        int8_t rssi;
        char local_name[32];
        char service_uuids[BLE_ADAPTER_MAX_SCAN_SERVICE_UUIDS][37];
        uint8_t service_uuid_count;
    } ble_adapter_scan_result_t;

/* Events queued by btstack callbacks (bt_task context) for later draining
 * by BLE.poll() (sketch task context). Later slices extend this
 * enum/struct further (e.g. with connection handles, attribute handles,
 * characteristic values, ...). */
    typedef enum {
        BLE_ADAPTER_EVENT_STACK_ENABLED = 0,
        BLE_ADAPTER_EVENT_STACK_DISABLED,
        BLE_ADAPTER_EVENT_SCAN_RESULT,
    } ble_adapter_event_type_t;

    typedef struct {
        ble_adapter_event_type_t type;
        /* Valid only when type == BLE_ADAPTER_EVENT_SCAN_RESULT. */
        ble_adapter_scan_result_t scan_result;
    } ble_adapter_event_t;

    class BLEAdapter {
    public:
        /* The adapter is implemented as a singleton, mirroring the public BLE
         * facade and other libraries (e.g. WiFiClass) in this codebase. */
        static BLEAdapter & instance();

        /* Initializes the platform BT configuration and the btstack stack
         * (wiced_bt_stack_init) and blocks (bounded by a timeout) until the
         * stack reports it is enabled. Returns true on success. */
        bool init(const char *device_name);

        /* Ends the current library session and discards queued events. The
         * BLESS-IPC stack remains initialized because its deinit/init cycle is
         * not restart-safe; a later init() reuses the running stack. */
        bool deinit();

        /* Non-blocking: pops a single queued event into 'event'. Returns false
         * if the queue is empty or the adapter was never initialized. Meant to
         * be called repeatedly from BLE.poll() until it returns false. */
        bool pop_event(ble_adapter_event_t &event);

        /* Builds a raw LE advertising payload (flags + optional local name +
         * optional advertised service UUID) from 'params' and starts
         * undirected connectable advertising. Returns false (see
         * last_error()) if the adapter isn't initialized, a UUID string
         * couldn't be parsed, or the underlying btstack calls fail. */
        bool start_advertising(const ble_adapter_advert_params_t &params);

        /* Stops any advertising started by start_advertising(). Safe to call
         * even if advertising was never started. */
        bool stop_advertising();

        /* Starts continuous LE scanning. Each discovered advertising packet
         * is queued as a BLE_ADAPTER_EVENT_SCAN_RESULT event for pop_event()
         * to drain. When 'service_uuid_filter' is non-null/non-empty, only
         * advertisements that include that service UUID are queued. Returns
         * false (see last_error()) if the adapter isn't initialized or the
         * underlying btstack call fails. */
        bool start_scan(const char *service_uuid_filter);

        /* Stops scanning started by start_scan(). Safe to call even if
         * scanning was never started. */
        bool stop_scan();

        bool is_initialized() const {
            return _initialized;
        }

        ble_adapter_error_t last_error() const {
            return _last_error;
        }

        /* Called by the file-local wiced_bt_management_cback_t trampoline in the
         * .cpp (registered with wiced_bt_stack_init, runs on the BLESS-IPC
         * bt_task context) whenever the stack becomes enabled/disabled. Public
         * so the trampoline (a plain C-linkage function, not a member) can call
         * them, but not part of the intended sketch-facing API. */
        void on_stack_enabled();
        void on_stack_disabled();

        /* Called by the file-local wiced_bt_ble_scan_result_cback_t
         * trampoline in the .cpp (registered with wiced_bt_ble_scan(), runs
         * on the BLESS-IPC bt_task context) for every discovered
         * advertising/scan-response packet. Public so the trampoline (a
         * plain C-linkage function, not a member) can call it, but not part
         * of the intended sketch-facing API. p_scan_result/p_adv_data are
         * NULL when btstack signals the end of a bounded scan; that is
         * ignored here since scanning is otherwise continuous until
         * stop_scan(). */
        void on_scan_result(const void *p_scan_result, const uint8_t *p_adv_data);

    private:
        BLEAdapter();
        ~BLEAdapter();
        BLEAdapter(const BLEAdapter &) = delete;
        BLEAdapter & operator = (const BLEAdapter &) = delete;

        void push_event(ble_adapter_event_type_t type);
        void push_scan_result_event(const ble_adapter_scan_result_t &scan_result);

        volatile bool _initialized;
        volatile bool _stack_init_started; /* wiced_bt_stack_init succeeded; enable event may still be pending */
        ble_adapter_error_t _last_error;

        /* Optional service UUID filter set by start_scan(); empty means "no
         * filter, queue every discovered device". */
        char _scan_service_uuid_filter[37];

        /* Active scanning elicits separate ADV_IND (flags/service UUIDs) and
         * SCAN_RSP (often the local name, e.g. ArduinoBLE's setLocalName())
         * reports for the same scannable peripheral. This single-slot cache
         * holds the most recent ADV_IND awaiting its SCAN_RSP so the two can
         * be merged into one BLEDevice before being queued; only one scan
         * result is normally in flight between consecutive controller
         * events, which is sufficient for this library's scan volume. */
        ble_adapter_scan_result_t _pending_scan_result;
        bool _pending_scan_result_valid;

        void maybe_queue_scan_result(const ble_adapter_scan_result_t &scan_result);

        QueueHandle_t _event_queue;

        /* Used only to make init()/deinit() synchronous: signaled by
         * management_callback() when the corresponding lifecycle event arrives. */
        SemaphoreHandle_t _lifecycle_sem;
    };

} // namespace ble_internal

#endif /* BLE_INTERNAL_BLE_ADAPTER_H */

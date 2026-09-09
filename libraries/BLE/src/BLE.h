#ifndef BLE_H
#define BLE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Public error codes for the BLE library. Methods that can fail return
 * bool (not exceptions); call BLE.lastError() to find out why the most
 * recent call failed.
 */
typedef enum {
    BLE_ERROR_NONE = 0,
    BLE_ERROR_INIT_FAILED,
    BLE_ERROR_INIT_TIMEOUT,
    BLE_ERROR_ALREADY_INITIALIZED,
    BLE_ERROR_NOT_INITIALIZED,
    BLE_ERROR_DEINIT_FAILED,
    BLE_ERROR_DEINIT_TIMEOUT,
    BLE_ERROR_QUEUE_CREATE_FAILED,
    BLE_ERROR_SEMAPHORE_CREATE_FAILED,
    BLE_ERROR_UNKNOWN,
} ble_error_t;

/**
 * Public facade for Bluetooth Low Energy on PSOC6 BLE-capable boards
 * (e.g. CY8CPROTO-063-BLE).
 *
 * BLEClass never talks to the MTB BT Stack (btstack/btstack-integration)
 * directly. All stack interaction is delegated to the internal adapter
 * (see src/internal/ble_adapter.h); this class only owns the public,
 * Arduino-friendly API surface and lifecycle bookkeeping.
 */
class BLEClass {

public:

    /* Return the BLE class singleton. */
    static BLEClass & get_instance();

    /* Initializes the BLE stack. Blocks (bounded by an internal timeout)
     * until the stack reports it is ready. Returns true on success; on
     * failure, use lastError() to find out why. */
    bool begin();

    /* Ends the current BLE session and discards pending library events. The
     * platform stack remains available for a later begin(). Safe to call even
     * if begin() was never called or already failed. */
    void end();

    /* Drains and processes pending BLE events queued by the internal
     * adapter's btstack callbacks. Must be called regularly (e.g. once per
     * sketch loop() iteration) from the sketch's task for BLE to make
     * progress; this is how events delivered on the BLESS-IPC bt_task
     * context are safely handed off to application code. */
    void poll();

    /* Returns the reason the most recent failing call failed. */
    ble_error_t lastError() const;

private:

    /* The BLE class is implemented as a singleton. The constructor and
     * destructor are private. */
    BLEClass();
    ~BLEClass();

    /* Delete copy constructor and assignment operator. */
    BLEClass(const BLEClass &) = delete;
    BLEClass & operator = (const BLEClass &) = delete;

    bool _active = false;
    ble_error_t _last_error = BLE_ERROR_NONE;
};

/* The PSOC6 PDL device headers (cyble_*_device.h, pulled in transitively via
 * Arduino.h/cyhal) '#define BLE' as the BLESS peripheral register base
 * address macro. That collides with the public singleton name below, so it
 * must be undefined before declaring it. */
#ifdef BLE
#undef BLE
#endif

extern BLEClass & BLE;

#endif /* BLE_H */

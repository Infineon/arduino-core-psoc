#ifndef BLE_H
#define BLE_H

#include <stdbool.h>
#include <stdint.h>

#include "BLEService.h"
#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#include "BLEDevice.h"

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
    BLE_ERROR_TOO_MANY_SERVICES,
    BLE_ERROR_INVALID_UUID,
    BLE_ERROR_ADVERTISE_FAILED,
    BLE_ERROR_SCAN_FAILED,
    BLE_ERROR_INVALID_ADDRESS,
    BLE_ERROR_ALREADY_CONNECTED,
    BLE_ERROR_NOT_CONNECTED,
    BLE_ERROR_CONNECT_FAILED,
    BLE_ERROR_DISCONNECT_FAILED,
    BLE_ERROR_GATT_REGISTER_FAILED,
    BLE_ERROR_DISCOVERY_FAILED,
    BLE_ERROR_READ_FAILED,
    BLE_ERROR_WRITE_FAILED,
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

    /* Registers a locally-hosted GATT service (with its characteristics)
     * so it can be advertised/exposed once advertise() is called. The
     * caller retains ownership of 'service'; it (and the characteristics
     * added to it) must outlive this BLEClass instance. Returns false if
     * the maximum number of services has already been added. */
    bool addService(BLEService &service);

    /* Starts undirected connectable advertising with the given local name
     * and, optionally, an advertised service UUID (pass nullptr or "" to
     * omit it). Returns false on failure; see lastError(). */
    bool advertise(const char *localName, const char *serviceUuid = nullptr);

    /* Stops advertising started by advertise(). Safe to call even if
     * advertising was never started. */
    void stopAdvertise();

    /* Starts continuous scanning for nearby peripherals (central role).
     * When 'serviceUuid' is non-null/non-empty, only peripherals
     * advertising that service UUID are surfaced by available(). Discovered
     * peripherals accumulate in an internal buffer for available() to
     * drain; call poll() regularly while scanning for that buffer to fill.
     * Returns false on failure; see lastError(). */
    bool scan(const char *serviceUuid = nullptr);

    /* Stops scanning started by scan(). Safe to call even if scanning was
     * never started. */
    void stopScan();

    /* Returns the next discovered peripheral queued since the last
     * available() call, or a default-constructed (BLEDevice::hasAddress()
     * == false) BLEDevice if none is queued. Must be called after poll()
     * has had a chance to drain the internal adapter's scan results. */
    BLEDevice available();

    /* Returns the currently connected central's BLEDevice (peripheral
     * role), once a central has connected (e.g. after advertise()). Returns
     * a default-constructed (BLEDevice::hasAddress() == false) BLEDevice if
     * no central is connected, or if the current connection was instead
     * initiated locally via BLEDevice::connect() (central role). */
    BLEDevice central();

    /* True if a connection is currently established, in either role (a
     * central connected to this peripheral, or this device connected as
     * central to a peripheral via BLEDevice::connect()). */
    bool connected() const;

    /* Returns the reason the most recent failing call failed. */
    ble_error_t lastError() const;

    /* Internal-use only: called by BLEDevice::connect()/disconnect() so
     * their result surfaces through BLE.lastError() (mapped from the
     * internal adapter's error code when 'success' is false), keeping a
     * single last-error convention across the public API surface even
     * though BLEDevice - not BLEClass - drives those two adapter calls.
     * Not part of the sketch-facing API. Returns 'success' unchanged. */
    bool _reportConnectionResult(bool success);

private:

    /* The BLE class is implemented as a singleton. The constructor and
     * destructor are private. */
    BLEClass();
    ~BLEClass();

    /* Delete copy constructor and assignment operator. */
    BLEClass(const BLEClass &) = delete;
    BLEClass & operator = (const BLEClass &) = delete;

    static const int MAX_SERVICES = 4;

    /* Bounds the internal buffer of discovered-but-not-yet-retrieved
     * devices that poll() fills from the adapter's queued scan results and
     * available() drains. */
    static const int MAX_DISCOVERED_DEVICES = 8;

    bool _active = false;
    ble_error_t _last_error = BLE_ERROR_NONE;
    BLEService *_services[MAX_SERVICES];
    int _serviceCount = 0;

    /* Simple FIFO ring buffer of discovered devices awaiting available().
     * If it fills up before the sketch calls available(), the oldest
     * not-yet-retrieved discovery is dropped in favor of the newest one. */
    BLEDevice _discoveredDevices[MAX_DISCOVERED_DEVICES];
    int _discoveredCount = 0;
    int _discoveredHead = 0;

    void _queueDiscoveredDevice(const BLEDevice &device);
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

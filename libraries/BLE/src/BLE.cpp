#include "BLE.h"

#include "internal/ble_adapter.h"

using ble_internal::BLEAdapter;
using ble_internal::ble_adapter_error_t;
using ble_internal::ble_adapter_event_t;

namespace {

/* Maps the internal adapter's error codes onto the public ble_error_t so
 * sketch authors never need to know about the internal adapter. */
    ble_error_t map_adapter_error(ble_adapter_error_t error) {
        switch (error) {
            case ble_internal::BLE_ADAPTER_ERROR_NONE:
                return BLE_ERROR_NONE;
            case ble_internal::BLE_ADAPTER_ERROR_ALREADY_INITIALIZED:
                return BLE_ERROR_ALREADY_INITIALIZED;
            case ble_internal::BLE_ADAPTER_ERROR_NOT_INITIALIZED:
                return BLE_ERROR_NOT_INITIALIZED;
            case ble_internal::BLE_ADAPTER_ERROR_STACK_INIT_FAILED:
                return BLE_ERROR_INIT_FAILED;
            case ble_internal::BLE_ADAPTER_ERROR_STACK_INIT_TIMEOUT:
                return BLE_ERROR_INIT_TIMEOUT;
            case ble_internal::BLE_ADAPTER_ERROR_STACK_DEINIT_FAILED:
                return BLE_ERROR_DEINIT_FAILED;
            case ble_internal::BLE_ADAPTER_ERROR_STACK_DEINIT_TIMEOUT:
                return BLE_ERROR_DEINIT_TIMEOUT;
            case ble_internal::BLE_ADAPTER_ERROR_QUEUE_CREATE_FAILED:
                return BLE_ERROR_QUEUE_CREATE_FAILED;
            case ble_internal::BLE_ADAPTER_ERROR_SEMAPHORE_CREATE_FAILED:
                return BLE_ERROR_SEMAPHORE_CREATE_FAILED;
            case ble_internal::BLE_ADAPTER_ERROR_INVALID_UUID:
                return BLE_ERROR_INVALID_UUID;
            case ble_internal::BLE_ADAPTER_ERROR_ADVERTISE_DATA_FAILED:
            case ble_internal::BLE_ADAPTER_ERROR_ADVERTISE_START_FAILED:
                return BLE_ERROR_ADVERTISE_FAILED;
            case ble_internal::BLE_ADAPTER_ERROR_SCAN_START_FAILED:
            case ble_internal::BLE_ADAPTER_ERROR_SCAN_STOP_FAILED:
                return BLE_ERROR_SCAN_FAILED;
            default:
                return BLE_ERROR_UNKNOWN;
        }
    }

/* Builds a BLEDevice from a queued scan result event. */
    BLEDevice make_discovered_device(const ble_internal::ble_adapter_scan_result_t &scan_result) {
        BLEDevice device;
        device._setAddress(scan_result.address);
        device._setLocalName(scan_result.local_name);
        device._setRssi(scan_result.rssi);
        device._clearAdvertisedServiceUuids();
        for (uint8_t i = 0; i < scan_result.service_uuid_count; i++) {
            device._addAdvertisedServiceUuid(scan_result.service_uuids[i]);
        }
        return device;
    }

} // namespace

BLEClass::BLEClass()
    : _active(false), _last_error(BLE_ERROR_NONE), _serviceCount(0),
    _discoveredCount(0), _discoveredHead(0) {
    for (int i = 0; i < MAX_SERVICES; i++) {
        _services[i] = nullptr;
    }
}

BLEClass::~BLEClass() {
    end();
}

BLEClass & BLEClass::get_instance() {
    static BLEClass instance;
    return instance;
}

bool BLEClass::begin() {
    if (_active) {
        _last_error = BLE_ERROR_ALREADY_INITIALIZED;
        return false;
    }

    if (!BLEAdapter::instance().init("PSOC6-BLE")) {
        _last_error = map_adapter_error(BLEAdapter::instance().last_error());
        return false;
    }

    _active = true;
    _last_error = BLE_ERROR_NONE;
    return true;
}

void BLEClass::end() {
    if (!_active) {
        return;
    }

    if (!BLEAdapter::instance().deinit()) {
        _last_error = map_adapter_error(BLEAdapter::instance().last_error());
    }

    _active = false;
}

void BLEClass::poll() {
    if (!_active) {
        return;
    }

    ble_adapter_event_t event;
    while (BLEAdapter::instance().pop_event(event)) {
        if (event.type == ble_internal::BLE_ADAPTER_EVENT_SCAN_RESULT) {
            _queueDiscoveredDevice(make_discovered_device(event.scan_result));
        }
        /* Other event types (stack enabled/disabled) have nothing
         * application-visible to do here yet; later slices (connections/
         * GATT) extend this. */
    }
}

ble_error_t BLEClass::lastError() const {
    return _last_error;
}

bool BLEClass::addService(BLEService &service) {
    if (_serviceCount >= MAX_SERVICES) {
        _last_error = BLE_ERROR_TOO_MANY_SERVICES;
        return false;
    }

    _services[_serviceCount++] = &service;
    _last_error = BLE_ERROR_NONE;
    return true;
}

bool BLEClass::advertise(const char *localName, const char *serviceUuid) {
    if (!_active) {
        _last_error = BLE_ERROR_NOT_INITIALIZED;
        return false;
    }

    ble_internal::ble_adapter_advert_params_t params;
    params.local_name = localName;
    params.service_uuid = serviceUuid;

    if (!BLEAdapter::instance().start_advertising(params)) {
        _last_error = map_adapter_error(BLEAdapter::instance().last_error());
        return false;
    }

    _last_error = BLE_ERROR_NONE;
    return true;
}

void BLEClass::stopAdvertise() {
    if (!_active) {
        return;
    }

    if (!BLEAdapter::instance().stop_advertising()) {
        _last_error = map_adapter_error(BLEAdapter::instance().last_error());
    }
}

bool BLEClass::scan(const char *serviceUuid) {
    if (!_active) {
        _last_error = BLE_ERROR_NOT_INITIALIZED;
        return false;
    }

    /* Start with a clean slate: discard any devices queued from a previous
     * scan session so available() only ever returns fresh results. */
    _discoveredCount = 0;
    _discoveredHead = 0;

    if (!BLEAdapter::instance().start_scan(serviceUuid)) {
        _last_error = map_adapter_error(BLEAdapter::instance().last_error());
        return false;
    }

    _last_error = BLE_ERROR_NONE;
    return true;
}

void BLEClass::stopScan() {
    if (!_active) {
        return;
    }

    if (!BLEAdapter::instance().stop_scan()) {
        _last_error = map_adapter_error(BLEAdapter::instance().last_error());
    }
}

BLEDevice BLEClass::available() {
    if (_discoveredCount == 0) {
        return BLEDevice();
    }

    BLEDevice device = _discoveredDevices[_discoveredHead];
    _discoveredHead = (_discoveredHead + 1) % MAX_DISCOVERED_DEVICES;
    _discoveredCount--;
    return device;
}

void BLEClass::_queueDiscoveredDevice(const BLEDevice &device) {
    /* Fixed-size ring buffer: if it is full, drop the oldest not-yet-
     * retrieved discovery in favor of this newer one rather than losing the
     * most recent (and most relevant) scan result. */
    if (_discoveredCount >= MAX_DISCOVERED_DEVICES) {
        _discoveredHead = (_discoveredHead + 1) % MAX_DISCOVERED_DEVICES;
        _discoveredCount--;
    }

    int tailIndex = (_discoveredHead + _discoveredCount) % MAX_DISCOVERED_DEVICES;
    _discoveredDevices[tailIndex] = device;
    _discoveredCount++;
}

/* See BLE.h: the PSOC6 PDL device headers '#define BLE' as a register base
 * address macro that collides with the singleton name. */
#ifdef BLE
#undef BLE
#endif

BLEClass & BLE = BLEClass::get_instance();

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
            default:
                return BLE_ERROR_UNKNOWN;
        }
    }

/* Handles a single event drained from the internal adapter's queue.
 * The scaffold/lifecycle slice has nothing application-visible to do with
 * these yet; later slices (advertising/connections/GATT) extend this. */
    void handle_adapter_event(const ble_adapter_event_t &event) {
        (void)event;
    }

} // namespace

BLEClass::BLEClass()
    : _active(false), _last_error(BLE_ERROR_NONE), _serviceCount(0) {
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
        handle_adapter_event(event);
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

/* See BLE.h: the PSOC6 PDL device headers '#define BLE' as a register base
 * address macro that collides with the singleton name. */
#ifdef BLE
#undef BLE
#endif

BLEClass & BLE = BLEClass::get_instance();

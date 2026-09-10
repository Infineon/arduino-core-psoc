#include "BLEDevice.h"

#include "BLE.h"
#include "BLEService.h"
#include "internal/ble_adapter.h"

#include <string.h>
#include <strings.h>

/* internal/ble_adapter.h (included above) pulls in FreeRTOSConfig.h ->
 * the PSOC6 PDL device headers, which '#define BLE' as the BLESS
 * peripheral register base address macro - re-polluting the macro after
 * BLE.h's own (earlier) undef. Must be undone again here, immediately
 * before this file's uses of the "BLE" singleton below (see BLE.h). */
#ifdef BLE
#undef BLE
#endif

BLEDevice::BLEDevice()
    : _rssi(0), _serviceUuidCount(0) {
    _address[0] = '\0';
    _localName[0] = '\0';
}

const char * BLEDevice::address() const {
    return _address;
}

bool BLEDevice::hasAddress() const {
    return _address[0] != '\0';
}

const char * BLEDevice::localName() const {
    return _localName;
}

bool BLEDevice::hasLocalName() const {
    return _localName[0] != '\0';
}

int BLEDevice::rssi() const {
    return _rssi;
}

int BLEDevice::advertisedServiceUuidCount() const {
    return _serviceUuidCount;
}

const char * BLEDevice::advertisedServiceUuid(int index) const {
    if (index < 0 || index >= _serviceUuidCount) {
        return "";
    }
    return _serviceUuids[index];
}

bool BLEDevice::hasAdvertisedServiceUuid(const char *uuid) const {
    if (uuid == nullptr) {
        return false;
    }
    for (int i = 0; i < _serviceUuidCount; i++) {
        if (strcasecmp(_serviceUuids[i], uuid) == 0) {
            return true;
        }
    }
    return false;
}

bool BLEDevice::connect() {
    if (!hasAddress()) {
        return false;
    }
    return BLE._reportConnectionResult(ble_internal::BLEAdapter::instance().connect(_address));
}

bool BLEDevice::disconnect() {
    if (!connected()) {
        return true;
    }
    return BLE._reportConnectionResult(ble_internal::BLEAdapter::instance().disconnect());
}

bool BLEDevice::connected() const {
    ble_internal::BLEAdapter &adapter = ble_internal::BLEAdapter::instance();
    return hasAddress() && adapter.is_connected()
           && strcasecmp(adapter.connected_address(), _address) == 0;
}

bool BLEDevice::discoverAttributes() {
    if (!connected()) {
        return false;
    }
    return BLE._reportConnectionResult(ble_internal::BLEAdapter::instance().discover_attributes());
}

int BLEDevice::serviceCount() const {
    return ble_internal::BLEAdapter::instance().discovered_service_count();
}

BLEService * BLEDevice::service(int index) const {
    return ble_internal::BLEAdapter::instance().discovered_service(index);
}

BLEService * BLEDevice::service(const char *uuid) const {
    return ble_internal::BLEAdapter::instance().find_discovered_service(uuid);
}

void BLEDevice::_setAddress(const char *address) {
    if (address == nullptr) {
        _address[0] = '\0';
        return;
    }
    strncpy(_address, address, sizeof(_address) - 1);
    _address[sizeof(_address) - 1] = '\0';
}

void BLEDevice::_setLocalName(const char *localName) {
    if (localName == nullptr) {
        _localName[0] = '\0';
        return;
    }
    strncpy(_localName, localName, sizeof(_localName) - 1);
    _localName[sizeof(_localName) - 1] = '\0';
}

void BLEDevice::_setRssi(int rssi) {
    _rssi = rssi;
}

void BLEDevice::_clearAdvertisedServiceUuids() {
    _serviceUuidCount = 0;
}

bool BLEDevice::_addAdvertisedServiceUuid(const char *uuid) {
    if (uuid == nullptr || _serviceUuidCount >= MAX_ADVERTISED_SERVICE_UUIDS) {
        return false;
    }
    strncpy(_serviceUuids[_serviceUuidCount], uuid, sizeof(_serviceUuids[_serviceUuidCount]) - 1);
    _serviceUuids[_serviceUuidCount][sizeof(_serviceUuids[_serviceUuidCount]) - 1] = '\0';
    _serviceUuidCount++;
    return true;
}

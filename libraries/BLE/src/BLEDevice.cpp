#include "BLEDevice.h"

#include <string.h>
#include <strings.h>

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

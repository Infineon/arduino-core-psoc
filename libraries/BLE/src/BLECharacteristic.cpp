#include "BLECharacteristic.h"
#include "BLE.h"
#include "internal/ble_adapter.h"

#include <string.h>

/* The PSOC6 device headers may define BLE as the BLESS peripheral register
 * macro; undo that collision before using the public BLE singleton. */
#ifdef BLE
#undef BLE
#endif

BLECharacteristic::BLECharacteristic(const char *uuid, uint8_t properties, int valueSize)
    : _properties(properties),
    _value(nullptr),
    _valueSize(valueSize > 0 ? valueSize : 1),
    _valueLength(0),
    _valueHandleField(0),
    _cccdHandleField(0),
    _remote(false),
    _written(false),
    _valueUpdated(false),
    _subscribed(false) {
    _uuid[0] = '\0';
    if (uuid != nullptr) {
        strncpy(_uuid, uuid, sizeof(_uuid) - 1);
        _uuid[sizeof(_uuid) - 1] = '\0';
    }

    _value = new uint8_t[_valueSize];
    memset(_value, 0, _valueSize);
}

BLECharacteristic::~BLECharacteristic() {
    delete[] _value;
}

const char * BLECharacteristic::uuid() const {
    return _uuid;
}

uint8_t BLECharacteristic::properties() const {
    return _properties;
}

int BLECharacteristic::valueSize() const {
    return _valueSize;
}

int BLECharacteristic::valueLength() const {
    return _valueLength;
}

const uint8_t * BLECharacteristic::value() const {
    return _value;
}

int BLECharacteristic::readValue(uint8_t *buffer, int length) const {
    if (buffer == nullptr || length <= 0) {
        return 0;
    }

    if (_remote) {
        int bytesRead = 0;
        if (!BLE._reportConnectionResult(
            ble_internal::BLEAdapter::instance().read_remote_characteristic(
                _valueHandleField, buffer, length, bytesRead))) {
            return 0;
        }
        return bytesRead;
    }

    int copyLength = (length < _valueLength) ? length : _valueLength;
    memcpy(buffer, _value, copyLength);
    return copyLength;
}

bool BLECharacteristic::writeValue(const uint8_t *value, int length) {
    if (length < 0 || length > _valueSize) {
        return false;
    }

    if (_remote) {
        if (!BLE._reportConnectionResult(
            ble_internal::BLEAdapter::instance().write_remote_characteristic(
                _valueHandleField, value, length))) {
            return false;
        }
    }

    if (length > 0 && value != nullptr) {
        memcpy(_value, value, length);
    }
    _valueLength = length;

    if (!_remote && _subscribed && (_properties & (BLENotify | BLEIndicate))) {
        bool indicate = (_properties & BLEIndicate) != 0;
        ble_internal::BLEAdapter::instance().notify_characteristic_value(_valueHandleField, _value, _valueLength, indicate);
    }

    return true;
}

bool BLECharacteristic::written() {
    if (!_written) {
        return false;
    }
    _written = false;
    return true;
}

bool BLECharacteristic::subscribe() {
    if (!_remote || _cccdHandleField == 0) {
        return false;
    }

    /* Prefer notifications when both BLENotify and BLEIndicate are set,
     * matching most peripherals' expectations (indications add an
     * acknowledgement round-trip that isn't needed unless the peripheral
     * only declared BLEIndicate). */
    uint16_t cccd_value = (_properties & BLENotify) ? 0x0001 : 0x0002;
    uint8_t buffer[2] = { (uint8_t)(cccd_value & 0xFF), (uint8_t)((cccd_value >> 8) & 0xFF) };
    return BLE._reportConnectionResult(
        ble_internal::BLEAdapter::instance().write_remote_characteristic(
            _cccdHandleField, buffer, sizeof(buffer)));
}

bool BLECharacteristic::unsubscribe() {
    if (!_remote || _cccdHandleField == 0) {
        return false;
    }

    uint8_t buffer[2] = { 0x00, 0x00 };
    return BLE._reportConnectionResult(
        ble_internal::BLEAdapter::instance().write_remote_characteristic(
            _cccdHandleField, buffer, sizeof(buffer)));
}

bool BLECharacteristic::valueUpdated() {
    if (!_valueUpdated) {
        return false;
    }
    _valueUpdated = false;
    return true;
}

bool BLECharacteristic::subscribed() const {
    return _subscribed;
}

bool BLECharacteristic::isRemote() const {
    return _remote;
}

bool BLECharacteristic::writeValue(const char *value) {
    if (value == nullptr) {
        return writeValue((const uint8_t *)nullptr, 0);
    }
    return writeValue((const uint8_t *)value, (int)strlen(value));
}

void BLECharacteristic::_setValueHandle(uint16_t handle) {
    _valueHandleField = handle;
}

uint16_t BLECharacteristic::_valueHandle() const {
    return _valueHandleField;
}

void BLECharacteristic::_setCccdHandle(uint16_t handle) {
    _cccdHandleField = handle;
}

uint16_t BLECharacteristic::_cccdHandle() const {
    return _cccdHandleField;
}

void BLECharacteristic::_setRemote(bool remote) {
    _remote = remote;
}

void BLECharacteristic::_setValueFromPeer(const uint8_t *value, int length) {
    if (length < 0) {
        return;
    }
    if (length > _valueSize) {
        length = _valueSize;
    }
    if (length > 0 && value != nullptr) {
        memcpy(_value, value, length);
    }
    _valueLength = length;
    _written = true;
}

void BLECharacteristic::_setValueFromNotification(const uint8_t *value, int length) {
    if (length < 0) {
        return;
    }
    if (length > _valueSize) {
        length = _valueSize;
    }
    if (length > 0 && value != nullptr) {
        memcpy(_value, value, length);
    }
    _valueLength = length;
    _valueUpdated = true;
}

void BLECharacteristic::_setSubscribed(bool subscribed) {
    _subscribed = subscribed;
}

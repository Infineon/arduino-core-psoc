#include "BLECharacteristic.h"

#include <string.h>

BLECharacteristic::BLECharacteristic(const char *uuid, uint8_t properties, int valueSize)
    : _properties(properties),
      _value(nullptr),
      _valueSize(valueSize > 0 ? valueSize : 1),
      _valueLength(0),
      _valueHandleField(0),
      _cccdHandleField(0) {
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

    int copyLength = (length < _valueLength) ? length : _valueLength;
    memcpy(buffer, _value, copyLength);
    return copyLength;
}

bool BLECharacteristic::writeValue(const uint8_t *value, int length) {
    if (length < 0 || length > _valueSize) {
        return false;
    }

    if (length > 0 && value != nullptr) {
        memcpy(_value, value, length);
    }
    _valueLength = length;
    return true;
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

#include "BLEDescriptor.h"

#include <string.h>

BLEDescriptor::BLEDescriptor(const char *uuid, const uint8_t *value, int valueLength)
    : _value(nullptr), _valueLength(valueLength > 0 ? valueLength : 0) {
    _uuid[0] = '\0';
    if (uuid != nullptr) {
        strncpy(_uuid, uuid, sizeof(_uuid) - 1);
        _uuid[sizeof(_uuid) - 1] = '\0';
    }

    if (_valueLength > 0) {
        _value = new uint8_t[_valueLength];
        if (value != nullptr) {
            memcpy(_value, value, _valueLength);
        } else {
            memset(_value, 0, _valueLength);
        }
    }
}

BLEDescriptor::~BLEDescriptor() {
    delete[] _value;
}

const char * BLEDescriptor::uuid() const {
    return _uuid;
}

const uint8_t * BLEDescriptor::value() const {
    return _value;
}

int BLEDescriptor::valueLength() const {
    return _valueLength;
}

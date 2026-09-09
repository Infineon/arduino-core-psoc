#include "BLEService.h"

#include <string.h>
#include <strings.h>

BLEService::BLEService(const char *uuid)
    : _characteristicCount(0), _startHandleField(0), _endHandleField(0) {
    _uuid[0] = '\0';
    if (uuid != nullptr) {
        strncpy(_uuid, uuid, sizeof(_uuid) - 1);
        _uuid[sizeof(_uuid) - 1] = '\0';
    }

    for (int i = 0; i < MAX_CHARACTERISTICS; i++) {
        _characteristics[i] = nullptr;
    }
}

const char * BLEService::uuid() const {
    return _uuid;
}

bool BLEService::addCharacteristic(BLECharacteristic &characteristic) {
    if (_characteristicCount >= MAX_CHARACTERISTICS) {
        return false;
    }

    _characteristics[_characteristicCount++] = &characteristic;
    return true;
}

int BLEService::characteristicCount() const {
    return _characteristicCount;
}

BLECharacteristic * BLEService::characteristic(int index) const {
    if (index < 0 || index >= _characteristicCount) {
        return nullptr;
    }
    return _characteristics[index];
}

BLECharacteristic * BLEService::characteristic(const char *uuid) const {
    if (uuid == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < _characteristicCount; i++) {
        if (strcasecmp(_characteristics[i]->uuid(), uuid) == 0) {
            return _characteristics[i];
        }
    }
    return nullptr;
}

void BLEService::_setHandleRange(uint16_t startHandle, uint16_t endHandle) {
    _startHandleField = startHandle;
    _endHandleField = endHandle;
}

uint16_t BLEService::_startHandle() const {
    return _startHandleField;
}

uint16_t BLEService::_endHandle() const {
    return _endHandleField;
}

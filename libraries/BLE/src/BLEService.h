#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "BLECharacteristic.h"

/**
 * A GATT service definition: a UUID plus the collection of
 * BLECharacteristic objects exposed under it. Sketch authors construct a
 * BLEService, add BLECharacteristic objects to it with addCharacteristic(),
 * and add the service to BLE (see BLE::addService()) before advertising it
 * with BLE::advertise().
 *
 * BLEService never talks to the internal adapter or btstack directly;
 * BLE::advertise() reaches into it (via the characteristics it holds) when
 * registering the local GATT server with the internal adapter.
 */
class BLEService {

public:

    /* Maximum number of characteristics a single service can hold. */
    static const int MAX_CHARACTERISTICS = 8;

    /* uuid: a 16-bit UUID string (e.g. "180D") or a 128-bit UUID string
     * (e.g. "19b10000-e8f2-537e-4f6c-d104768a1214"). */
    explicit BLEService(const char *uuid);

    BLEService(const BLEService &) = delete;
    BLEService & operator = (const BLEService &) = delete;

    const char * uuid() const;

    /* Adds a characteristic to this service. Returns false if the service
     * has reached MAX_CHARACTERISTICS. The caller retains ownership of
     * 'characteristic'; it must outlive the service (and BLE.advertise()
     * calls that reference it). */
    bool addCharacteristic(BLECharacteristic &characteristic);

    int characteristicCount() const;

    /* Returns the characteristic at 'index' (0 <= index < characteristicCount()),
     * or nullptr if out of range. */
    BLECharacteristic * characteristic(int index) const;

private:

    char _uuid[37]; /* Fits a 128-bit UUID string ("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"). */
    BLECharacteristic *_characteristics[MAX_CHARACTERISTICS];
    int _characteristicCount;
};

#endif /* BLE_SERVICE_H */

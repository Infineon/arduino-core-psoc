#ifndef BLE_DESCRIPTOR_H
#define BLE_DESCRIPTOR_H

#include <stdint.h>

/**
 * A GATT descriptor: a UUID plus a fixed-size value buffer, e.g. the
 * Client Characteristic Configuration Descriptor (CCCD, UUID "2902") that
 * every BLENotify/BLEIndicate BLECharacteristic implicitly gets (see
 * BLECharacteristic::subscribe()/unsubscribe()/subscribed()).
 *
 * This slice (issues/006-notifications-subscriptions.md) only needs
 * BLEDescriptor as a plain UUID+value data holder matching the PRD's GATT
 * object model; the CCCD itself is tracked internally by BLECharacteristic/
 * BLEAdapter (handle assignment, subscribe state) rather than through a
 * BLEDescriptor instance, so BLEDescriptor never talks to the internal
 * adapter or btstack directly.
 */
class BLEDescriptor {

public:

    /* uuid: a 16-bit or 128-bit UUID string (e.g.
     * "19b10000-e8f2-537e-4f6c-d104768a1214").
     * value/valueLength: the descriptor's initial value, copied into an
     * internally-owned buffer (up to valueLength bytes). */
    BLEDescriptor(const char *uuid, const uint8_t * value, int valueLength);
    ~BLEDescriptor();

    BLEDescriptor(const BLEDescriptor &) = delete;
    BLEDescriptor & operator = (const BLEDescriptor &) = delete;

    const char * uuid() const;

    /* Read-only access to the descriptor's value buffer contents. */
    const uint8_t * value() const;

    /* Number of bytes currently stored in the value buffer. */
    int valueLength() const;

private:

    char _uuid[37]; /* Fits a 128-bit UUID string ("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"). */
    uint8_t *_value;
    int _valueLength;
};

#endif /* BLE_DESCRIPTOR_H */

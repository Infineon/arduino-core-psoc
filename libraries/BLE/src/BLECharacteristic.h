#ifndef BLE_CHARACTERISTIC_H
#define BLE_CHARACTERISTIC_H

#include <stdint.h>

/**
 * GATT characteristic property bits, matching the Bluetooth Core
 * Specification (Vol 3, Part G, 3.3.1.1) bit positions used by the
 * internal adapter/btstack layer, so no translation is needed when the
 * value is handed to the adapter.
 */
enum {
    BLEBroadcast          = 0x01,
    BLERead               = 0x02,
    BLEWriteWithoutResponse = 0x04,
    BLEWrite              = 0x08,
    BLENotify             = 0x10,
    BLEIndicate           = 0x20,
};

/**
 * A single GATT characteristic belonging to a BLEService: a UUID, a set of
 * properties (read/write/notify, see the BLE* property constants above),
 * and a fixed-size value buffer that sketch authors read/write through
 * writeValue()/value().
 *
 * BLECharacteristic never talks to the internal adapter or btstack
 * directly; the adapter reaches into it (via the internal handle setters)
 * once it is registered with the GATT server through BLEService/BLE.
 */
class BLECharacteristic {

public:

    /* uuid: a 16-bit UUID string (e.g. "2A37") or a 128-bit UUID string
     * (e.g. "19b10000-e8f2-537e-4f6c-d104768a1214"). valueSize bounds the
     * characteristic's value buffer in bytes (default matches ArduinoBLE's
     * common default of 20 bytes, the default ATT MTU payload). */
    BLECharacteristic(const char *uuid, uint8_t properties, int valueSize = 20);
    ~BLECharacteristic();

    BLECharacteristic(const BLECharacteristic &) = delete;
    BLECharacteristic & operator = (const BLECharacteristic &) = delete;

    const char * uuid() const;
    uint8_t properties() const;

    /* Maximum number of bytes the value buffer can hold. */
    int valueSize() const;

    /* Number of bytes currently stored in the value buffer. */
    int valueLength() const;

    /* Read-only access to the current value buffer contents. */
    const uint8_t * value() const;

    /* Copies up to 'length' bytes of the current value into 'buffer'.
     * Returns the number of bytes copied. On a characteristic discovered by
     * BLEDevice::discoverAttributes() (see isRemote()), this instead
     * performs a blocking GATT read of the connected peripheral's
     * characteristic (central role) directly into 'buffer', returning the
     * number of bytes the peer responded with, or 0 on failure/timeout
     * (see BLE.lastError()). */
    int readValue(uint8_t *buffer, int length) const;

    /* Replaces the value buffer contents. On a characteristic discovered by
     * BLEDevice::discoverAttributes() (see isRemote()), this also performs a
     * blocking GATT write to the connected peripheral (central role) before
     * updating the local cache; returns false if that write fails/times out
     * (see BLE.lastError()). Returns false (and leaves the value unchanged)
     * if 'length' exceeds valueSize(). */
    bool writeValue(const uint8_t *value, int length);
    bool writeValue(const char *value);

    /* Peripheral side: true once since the last call to written() a
     * connected central has written a new value to this characteristic
     * (via the GATT server; see BLE::advertise()). Consumes the pending
     * flag: a second immediate call returns false until another write
     * arrives. Always false for a remote (centrally-discovered)
     * characteristic. */
    bool written();

    /* True if this characteristic represents a remote attribute discovered
     * by BLEDevice::discoverAttributes() (central role) rather than one
     * locally declared and added to a BLEService for advertising
     * (peripheral role, see BLEService::addCharacteristic()). readValue()/
     * writeValue() perform a live GATT read/write against the connected
     * peer only when this is true. */
    bool isRemote() const;

    /* Internal-use only: the GATT attribute handles assigned to this
     * characteristic's value (and, when BLENotify/BLEIndicate is set, its
     * Client Characteristic Configuration descriptor) once it has been
     * registered with the GATT server by BLE::advertise(), or discovered by
     * BLEDevice::discoverAttributes(). Not part of the sketch-facing API;
     * only the internal adapter calls these. */
    void _setValueHandle(uint16_t handle);
    uint16_t _valueHandle() const;
    void _setCccdHandle(uint16_t handle);
    uint16_t _cccdHandle() const;

    /* Internal-use only: marks this characteristic as remote (see
     * isRemote()). Only the internal adapter calls this, when constructing
     * the result of BLEDevice::discoverAttributes(). Not part of the
     * sketch-facing API. */
    void _setRemote(bool remote);

    /* Internal-use only: called by the internal adapter's GATT server
     * (GATT_ATTRIBUTE_REQUEST_EVT write handling) when a connected central
     * writes to this characteristic. Updates the value buffer and marks
     * written() to return true on its next call. Not part of the
     * sketch-facing API. */
    void _setValueFromPeer(const uint8_t *value, int length);

private:

    char _uuid[37]; /* Fits a 128-bit UUID string ("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"). */
    uint8_t _properties;
    uint8_t *_value;
    int _valueSize;
    int _valueLength;
    uint16_t _valueHandleField;
    uint16_t _cccdHandleField;
    bool _remote;
    bool _written;
};

#endif /* BLE_CHARACTERISTIC_H */

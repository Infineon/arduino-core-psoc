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
     * Returns the number of bytes copied. */
    int readValue(uint8_t *buffer, int length) const;

    /* Replaces the value buffer contents. Returns false (and leaves the
     * value unchanged) if 'length' exceeds valueSize(). */
    bool writeValue(const uint8_t *value, int length);
    bool writeValue(const char *value);

    /* Internal-use only: the GATT attribute handles assigned to this
     * characteristic's value (and, when BLENotify/BLEIndicate is set, its
     * Client Characteristic Configuration descriptor) once it has been
     * registered with the GATT server by BLE::advertise(). Not part of the
     * sketch-facing API; only the internal adapter calls these. */
    void _setValueHandle(uint16_t handle);
    uint16_t _valueHandle() const;
    void _setCccdHandle(uint16_t handle);
    uint16_t _cccdHandle() const;

private:

    char _uuid[37]; /* Fits a 128-bit UUID string ("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"). */
    uint8_t _properties;
    uint8_t *_value;
    int _valueSize;
    int _valueLength;
    uint16_t _valueHandleField;
    uint16_t _cccdHandleField;
};

#endif /* BLE_CHARACTERISTIC_H */

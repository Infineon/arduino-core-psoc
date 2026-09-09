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
     * if 'length' exceeds valueSize(). On a locally-declared (peripheral
     * role) characteristic with BLENotify/BLEIndicate in its properties()
     * and a currently subscribed() central, this also sends a notification/
     * indication of the new value to that central. */
    bool writeValue(const uint8_t *value, int length);
    bool writeValue(const char *value);

    /* Peripheral side: true once since the last call to written() a
     * connected central has written a new value to this characteristic
     * (via the GATT server; see BLE::advertise()). Consumes the pending
     * flag: a second immediate call returns false until another write
     * arrives. Always false for a remote (centrally-discovered)
     * characteristic. */
    bool written();

    /* Central role: subscribes to (BLENotify)/enables indications for
     * (BLEIndicate, preferred when both are set) this remote characteristic
     * by writing its discovered Client Characteristic Configuration
     * Descriptor (CCCD) on the connected peripheral. Requires this
     * characteristic to be remote (see isRemote(), set by
     * BLEDevice::discoverAttributes()) and to have a discovered CCCD handle
     * (i.e. BLENotify or BLEIndicate was set in its properties()). Blocks
     * (bounded by an internal timeout); returns false on failure/timeout
     * or if the preconditions above aren't met (see BLE.lastError()). */
    bool subscribe();

    /* Central role: disables notifications/indications previously enabled
     * by subscribe(), by writing zero to the CCCD. Same preconditions/
     * blocking behavior as subscribe(). */
    bool unsubscribe();

    /* Central role: true once since the last call to valueUpdated() a
     * notification/indication for this remote characteristic has been
     * received from the connected peripheral (requires an active
     * subscribe(); see BLE.poll(), which must be called regularly for
     * notifications to be received and dispatched). Consumes the pending
     * flag like written(). Always false for a locally-declared (peripheral
     * role) characteristic. */
    bool valueUpdated();

    /* Peripheral role: true while a connected central currently has
     * notifications and/or indications enabled for this characteristic
     * (i.e. has written a non-zero value to its CCCD - see
     * BLEAdapter::on_gatt_attribute_request()). Reflects current state
     * rather than consuming a pending flag (unlike written()). Sketch
     * authors should check this before calling writeValue() if they only
     * want to do the work of producing a new value when someone is
     * listening; writeValue() itself always sends a notification/
     * indication when this is true, regardless of whether the caller
     * checked first. Always false for a remote (centrally-discovered)
     * characteristic, or one without BLENotify/BLEIndicate in its
     * properties(). */
    bool subscribed() const;

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

    /* Internal-use only: called by the internal adapter (central role)
     * when a notification/indication for this remote characteristic
     * arrives (see BLEAdapter::on_gatt_operation_complete()). Updates the
     * value buffer and marks valueUpdated() to return true on its next
     * call; unlike _setValueFromPeer(), does not affect written(). Not
     * part of the sketch-facing API. */
    void _setValueFromNotification(const uint8_t *value, int length);

    /* Internal-use only: called by the internal adapter's GATT server
     * (GATT_ATTRIBUTE_REQUEST_EVT write handling) when a connected central
     * writes to this characteristic's CCCD, reflecting its current
     * subscribe state (see subscribed()). Not part of the sketch-facing
     * API. */
    void _setSubscribed(bool subscribed);

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
    bool _valueUpdated;
    bool _subscribed;
};

#endif /* BLE_CHARACTERISTIC_H */

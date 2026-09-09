#ifndef BLE_DEVICE_H
#define BLE_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * A remote BLE peripheral discovered by BLE.scan(), retrieved one at a time
 * via BLE.available(). Exposes the address/local name/RSSI/advertised
 * service UUIDs captured from that peripheral's advertising packet(s) at
 * the moment it was discovered.
 *
 * BLEDevice is a plain data holder filled in by BLEClass from the internal
 * adapter's scan results; it never talks to the internal adapter or
 * btstack directly.
 */
class BLEDevice {

public:

    /* Maximum number of advertised service UUIDs captured per discovered
     * device (bounds the internal adapter's fixed-size scan result event). */
    static const int MAX_ADVERTISED_SERVICE_UUIDS = 4;

    /* Constructs an invalid (no discovered device) BLEDevice: address(),
     * localName() return "" and rssi() returns 0. BLE.available() returns
     * such a default-constructed BLEDevice when nothing has been
     * discovered. */
    BLEDevice();

    /* The device's Bluetooth address, formatted as "AA:BB:CC:DD:EE:FF". */
    const char * address() const;

    /* True if this BLEDevice represents an actual discovered peripheral
     * (as opposed to the default-constructed "nothing available" value). */
    bool hasAddress() const;

    /* The peripheral's advertised local name, or "" if it did not advertise
     * one. */
    const char * localName() const;

    /* True if the peripheral advertised a local name. */
    bool hasLocalName() const;

    /* The received signal strength, in dBm, at the moment this device was
     * discovered. */
    int rssi() const;

    /* Number of advertised service UUIDs captured for this device (up to
     * MAX_ADVERTISED_SERVICE_UUIDS). */
    int advertisedServiceUuidCount() const;

    /* The advertised service UUID at 'index' (0 <= index <
     * advertisedServiceUuidCount()), or "" if out of range. */
    const char * advertisedServiceUuid(int index) const;

    /* True if the peripheral advertised the given service UUID (16-bit or
     * 128-bit UUID string, case-insensitive). */
    bool hasAdvertisedServiceUuid(const char *uuid) const;

    /* Internal-use only: fills in this BLEDevice's fields from a scan
     * result. Not part of the sketch-facing API; only BLEClass calls this
     * (see BLE::available()). */
    void _setAddress(const char *address);
    void _setLocalName(const char *localName);
    void _setRssi(int rssi);
    void _clearAdvertisedServiceUuids();
    bool _addAdvertisedServiceUuid(const char *uuid);

private:

    char _address[18]; /* "AA:BB:CC:DD:EE:FF\0" */
    char _localName[32];
    int _rssi;
    char _serviceUuids[MAX_ADVERTISED_SERVICE_UUIDS][37]; /* Fits a 128-bit UUID string. */
    int _serviceUuidCount;
};

#endif /* BLE_DEVICE_H */

#ifndef BLE_DEVICE_H
#define BLE_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

class BLEService;

/**
 * A remote BLE peer: either a peripheral discovered by BLE.scan() (and
 * retrieved one at a time via BLE.available()), or the connected central
 * returned by BLE.central() (peripheral role). Exposes the
 * address/local name/RSSI/advertised service UUIDs captured from a scanned
 * peripheral's advertising packet(s) at the moment it was discovered, and
 * connect()/disconnect()/connected() to establish and query a connection to
 * this peer.
 *
 * BLEDevice is a plain data holder filled in by BLEClass from the internal
 * adapter's scan results; connect()/disconnect()/connected() are the only
 * BLEDevice methods that reach into the internal adapter directly (there is
 * only ever one active connection at a time - see the PRD's "Connection
 * topology" decision - so the adapter, not this value type, is the source
 * of truth for current connection state).
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

    /* Establishes a connection (central role) to this device's address.
     * Blocks (bounded by an internal timeout) until the connection
     * completes. Only one connection is supported at a time (see the PRD's
     * "Connection topology" decision); returns false if this device has no
     * address, a connection is already active, or the connection attempt
     * fails/times out - see BLE.lastError(). */
    bool connect();

    /* Ends the connection to this device, if it is the currently connected
     * peer (in either role - this also works to end an incoming connection
     * from a central returned by BLE.central()). Blocks (bounded by an
     * internal timeout) until the disconnection completes. Safe to call
     * when this device isn't the connected peer (returns true, no-op). */
    bool disconnect();

    /* True if this device is the currently connected peer (in either
     * role). */
    bool connected() const;

    /* Central role only: performs a blocking GATT discovery of all
     * services and characteristics exposed by this device (which must be
     * the currently connected peer, i.e. connected() == true). Populates
     * the results returned by service()/serviceCount(). A repeated call
     * discards any previously discovered services/characteristics.
     * Returns false on failure/timeout (see BLE.lastError()); some
     * services may still have been partially discovered. */
    bool discoverAttributes();

    /* Number of services discovered by the most recent discoverAttributes()
     * call. */
    int serviceCount() const;

    /* Returns the discovered service at 'index' (0 <= index <
     * serviceCount()), or nullptr if out of range. */
    BLEService * service(int index) const;

    /* Returns the discovered service with the given UUID (16-bit or
     * 128-bit UUID string, case-insensitive), or nullptr if
     * discoverAttributes() found none with that UUID. */
    BLEService * service(const char *uuid) const;

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

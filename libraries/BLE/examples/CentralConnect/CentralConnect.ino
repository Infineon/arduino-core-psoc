/*
  CentralConnect

  Tracer-bullet example demonstrating central-side connection establishment:
  scans for a peripheral advertising the service UUID below, connects to the
  first one found, prints connection state while connected, and disconnects
  once BLE.connected() (via the BLEDevice) reports the link has dropped.

  This is a connect/disconnect-only slice - it does not yet discover
  attributes or read/write/subscribe to characteristics; those are added by
  later library slices (see the BLE library's ai-flow issues).

  This sketch requires a BLE-capable PSOC6 board (e.g. CY8CPROTO-063-BLE).
  Run it alongside a peripheral advertising the service UUID below, e.g.
  the PeripheralConnect example in this library or a standard ArduinoBLE
  peripheral sketch.
*/

#include <BLE.h>

const char kServiceUuid[] = "19b10000-e8f2-537e-4f6c-d104768a1214";

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  if (!BLE.begin()) {
    Serial.print("BLE.begin() failed, error code: ");
    Serial.println(BLE.lastError());
    while (1) {
      delay(1000);
    }
  }

  if (!BLE.scan(kServiceUuid)) {
    Serial.print("BLE.scan() failed, error code: ");
    Serial.println(BLE.lastError());
    while (1) {
      delay(1000);
    }
  }

  Serial.println("Scanning for a peripheral to connect to...");
}

void loop() {
  BLE.poll();

  BLEDevice peripheral = BLE.available();
  if (!peripheral.hasAddress()) {
    return;
  }

  BLE.stopScan();

  Serial.print("Connecting to ");
  Serial.println(peripheral.address());

  if (!peripheral.connect()) {
    Serial.print("connect() failed, error code: ");
    Serial.println(BLE.lastError());
    return;
  }

  Serial.println("Connected");

  while (peripheral.connected()) {
    BLE.poll();
    delay(500);
  }

  Serial.println("Disconnected");
  peripheral.disconnect();
}

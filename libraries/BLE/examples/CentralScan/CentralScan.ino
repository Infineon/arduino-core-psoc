/*
  CentralScan

  Tracer-bullet example demonstrating central-side scanning and discovery:
  starts a continuous scan filtered by service UUID and prints each
  discovered peripheral's address, local name, RSSI, and advertised service
  UUIDs.

  This is a scanning/discovery-only slice - it does not yet connect to a
  discovered peripheral; that is added by later library slices (see the BLE
  library's ai-flow issues).

  This sketch requires a BLE-capable PSOC6 board (e.g. CY8CPROTO-063-BLE).
  Run it alongside a peripheral advertising the service UUID below, e.g.
  the PeripheralAdvertise example in this library or a standard ArduinoBLE
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

  Serial.println("Scanning for peripherals...");
}

void loop() {
  BLE.poll();

  BLEDevice peripheral = BLE.available();
  if (peripheral.hasAddress()) {
    Serial.print("Found ");
    Serial.print(peripheral.address());
    Serial.print(" '");
    Serial.print(peripheral.localName());
    Serial.print("' RSSI ");
    Serial.print(peripheral.rssi());
    Serial.print(" services:");
    for (int i = 0; i < peripheral.advertisedServiceUuidCount(); i++) {
      Serial.print(" ");
      Serial.print(peripheral.advertisedServiceUuid(i));
    }
    Serial.println();
  }
}

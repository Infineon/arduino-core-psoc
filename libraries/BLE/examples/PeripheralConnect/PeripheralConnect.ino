/*
  PeripheralConnect

  Tracer-bullet example demonstrating peripheral-side connection handling:
  defines a BLEService with one BLECharacteristic, advertises it, and reacts
  to a central connecting/disconnecting via BLE.central()/BLE.connected().

  This is a connection-handling-only slice - it does not yet serve GATT
  reads/writes or notifications for the characteristic; those are added by
  later library slices (see the BLE library's ai-flow issues).

  This sketch requires a BLE-capable PSOC6 board (e.g. CY8CPROTO-063-BLE).
  Connect to it with the CentralConnect example in this library or a
  standard ArduinoBLE central sketch.
*/

#include <BLE.h>

BLEService ledService("19b10000-e8f2-537e-4f6c-d104768a1214");
BLECharacteristic ledCharacteristic("19b10001-e8f2-537e-4f6c-d104768a1214", BLERead | BLEWrite);

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

  ledService.addCharacteristic(ledCharacteristic);
  BLE.addService(ledService);

  if (!BLE.advertise("PSOC6BLE", ledService.uuid())) {
    Serial.print("BLE.advertise() failed, error code: ");
    Serial.println(BLE.lastError());
    while (1) {
      delay(1000);
    }
  }

  Serial.println("BLE advertising as PSOC6BLE, waiting for a central...");
}

void loop() {
  BLE.poll();

  BLEDevice central = BLE.central();
  if (!central.hasAddress()) {
    return;
  }

  Serial.print("Connected to central: ");
  Serial.println(central.address());

  while (central.connected()) {
    BLE.poll();
    delay(500);
  }

  Serial.println("Central disconnected, resuming advertising");
  BLE.advertise("PSOC6BLE", ledService.uuid());
}

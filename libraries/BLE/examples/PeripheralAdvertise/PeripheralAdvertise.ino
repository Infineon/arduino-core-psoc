/*
  PeripheralAdvertise

  Tracer-bullet example demonstrating peripheral-side GATT service
  definition and advertising: defines a BLEService with one BLECharacteristic,
  adds the service to BLE, and starts advertising it with a local name.

  This is a definition/advertising-only slice - it does not yet accept
  connections or serve GATT reads/writes; those are added by later library
  slices (see the BLE library's ai-flow issues).

  This sketch requires a BLE-capable PSOC6 board (e.g. CY8CPROTO-063-BLE).
  Discover it with a standard ArduinoBLE central sketch or a BLE scanner app
  and confirm it advertises the local name "PSOC6BLE" and the service UUID
  below.
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

  Serial.println("BLE advertising as PSOC6BLE");
}

void loop() {
  BLE.poll();
}

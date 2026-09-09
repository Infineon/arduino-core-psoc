/*
  BLEBeginEnd

  Minimal tracer-bullet example demonstrating the BLE library lifecycle:
  BLE.begin(), BLE.poll(), and BLE.end(). Later library slices add
  advertising, GATT services/characteristics, scanning, connections, and
  notifications on top of this foundation.

  This sketch requires a BLE-capable PSOC6 board (e.g. CY8CPROTO-063-BLE).
*/

#include <BLE.h>

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

  Serial.println("BLE started");
}

void loop() {
  BLE.poll();
}

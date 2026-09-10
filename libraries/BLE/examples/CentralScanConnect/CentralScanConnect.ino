/*
  CentralScanConnect

  Scans for and exercises PeripheralGATTServer, validating that service
  and characteristic UUIDs survive advertising, discovery, read/write,
  and subscription flows.
*/

#include <BLE.h>

const char kLedServiceUuid[] = "0003CBBB-0000-1000-8000-00805F9B0131";
const char kLedCharUuid[] = "0003CBB1-0000-1000-8000-00805F9B0131";
const char kDataServiceUuid[] = "33C94449-EA57-4047-A6C5-EA28E15A2642";
const char kCounterCharUuid[] = "E624B3AA-3397-4FC5-A7B0-4ACB2776687D";

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

  if (!BLE.scan(kLedServiceUuid)) {
    Serial.print("BLE.scan() failed, error code: ");
    Serial.println(BLE.lastError());
    while (1) {
      delay(1000);
    }
  }

  Serial.println("Scanning for a peripheral advertising the service...");
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

  if (!peripheral.discoverAttributes()) {
    Serial.print("discoverAttributes() failed, error code: ");
    Serial.println(BLE.lastError());
    peripheral.disconnect();
    return;
  }

  BLEService *ledService = peripheral.service(kLedServiceUuid);
  BLEService *dataService = peripheral.service(kDataServiceUuid);
  BLECharacteristic *ledCharacteristic =
      ledService == nullptr ? nullptr : ledService->characteristic(kLedCharUuid);
  BLECharacteristic *counterCharacteristic =
      dataService == nullptr ? nullptr : dataService->characteristic(kCounterCharUuid);
  if (ledService == nullptr || dataService == nullptr
      || ledCharacteristic == nullptr || counterCharacteristic == nullptr) {
    Serial.println("Expected service or characteristic was not discovered");
    peripheral.disconnect();
    return;
  }

  Serial.println("Discovered service and both characteristics");

  uint8_t ledOn = 1;
  if (!ledCharacteristic->writeValue(&ledOn, sizeof(ledOn))) {
    Serial.print("LED write failed, error code: ");
    Serial.println(BLE.lastError());
  }

  uint8_t readBuf[1];
  if (ledCharacteristic->readValue(readBuf, sizeof(readBuf)) > 0) {
    Serial.print("Read LED characteristic: ");
    Serial.println(readBuf[0]);
  }

  if (!counterCharacteristic->subscribe()) {
    Serial.print("Subscribe failed, error code: ");
    Serial.println(BLE.lastError());
  } else {
    Serial.println("Subscribed to counter notifications");
  }

  while (peripheral.connected()) {
    BLE.poll();
    if (counterCharacteristic->valueUpdated()) {
      Serial.print("Counter notification: ");
      Serial.println(counterCharacteristic->value()[0]);
    }
    delay(10);
  }

  Serial.println("Disconnected");
}

/*
  PeripheralGATTServer

  Uses the Infineon multi-peripheral example's custom characteristic UUID
  so this sketch can be used to validate GATT database construction,
  discovery, reads/writes, and notifications.

  Run this sketch on one BLE-capable PSOC6 board and
  CentralScanConnect on another.
*/

#include <BLE.h>

const char kLedServiceUuid[] = "0003CBBB-0000-1000-8000-00805F9B0131";
const char kLedCharUuid[] = "0003CBB1-0000-1000-8000-00805F9B0131";
const char kDataServiceUuid[] = "33C94449-EA57-4047-A6C5-EA28E15A2642";
const char kCounterCharUuid[] = "E624B3AA-3397-4FC5-A7B0-4ACB2776687D";

BLEService ledService(kLedServiceUuid);
BLEService dataService(kDataServiceUuid);
BLECharacteristic ledCharacteristic(kLedCharUuid, BLERead | BLEWrite);
BLECharacteristic counterCharacteristic(kCounterCharUuid, BLERead | BLENotify);

uint8_t counter = 0;
unsigned long lastCounterUpdate = 0;

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

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  ledService.addCharacteristic(ledCharacteristic);
  dataService.addCharacteristic(counterCharacteristic);
  BLE.addService(ledService);
  BLE.addService(dataService);

  uint8_t ledOff = 0;
  ledCharacteristic.writeValue(&ledOff, sizeof(ledOff));
  counterCharacteristic.writeValue(&counter, sizeof(counter));

  if (!BLE.advertise("PSOC6GATT", ledService.uuid())) {
    Serial.print("BLE.advertise() failed, error code: ");
    Serial.println(BLE.lastError());
    while (1) {
      delay(1000);
    }
  }

  Serial.println("BLE advertising GATT service, waiting for a central...");
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

    if (ledCharacteristic.written()) {
      bool ledOn = ledCharacteristic.value()[0] != 0;
      digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
      Serial.print("LED characteristic written: ");
      Serial.println(ledOn ? "on" : "off");
    }

    if (counterCharacteristic.subscribed() && millis() - lastCounterUpdate >= 1000) {
      lastCounterUpdate = millis();
      counter++;
      counterCharacteristic.writeValue(&counter, sizeof(counter));
      Serial.print("Notified counter: ");
      Serial.println(counter);
    }

    delay(10);
  }

  Serial.println("Central disconnected, resuming advertising");
  BLE.advertise("PSOC6GATT", ledService.uuid());
}

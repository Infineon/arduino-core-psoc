/*
  Copyright (c) 2011 Arduino.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/



// ****************************************************************************
// @Project Includes
// ****************************************************************************
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#if (ARDUINO_USB_CDC_ON_BOOT | ARDUINO_USB_MSC_ON_BOOT | ARDUINO_USB_DFU_ON_BOOT) && !ARDUINO_USB_MODE
#include "USB.h"
#if ARDUINO_USB_MSC_ON_BOOT
#include "FirmwareMSC.h"
#endif
#endif

// ****************************************************************************
// @Defines
// ****************************************************************************

#ifndef ARDUINO_LOOP_STACK_SIZE
#ifndef CONFIG_ARDUINO_LOOP_STACK_SIZE
#define ARDUINO_LOOP_STACK_SIZE 8192
#else
#define ARDUINO_LOOP_STACK_SIZE CONFIG_ARDUINO_LOOP_STACK_SIZE
#endif
#endif

TaskHandle_t loopTaskHandle = NULL;

#if CONFIG_AUTOSTART_ARDUINO
#if CONFIG_FREERTOS_UNICORE
void yieldIfNecessary(void) {
    static uint64_t lastYield = 0;
    uint64_t now = millis();
    if ((now - lastYield) > 2000) {
        lastYield = now;
        vTaskDelay(5); // delay 1 RTOS tick
    }
}
#endif

bool loopTaskWDTEnabled;

__attribute__((weak)) size_t getArduinoLoopTaskStackSize(void) {
    return ARDUINO_LOOP_STACK_SIZE;
}

void loopTask(void *pvParameters) {
    setup();
    for (;;) {
        #if CONFIG_FREERTOS_UNICORE
        yieldIfNecessary();
        #endif
        loop();
        if (serialEventRun) {
            serialEventRun();
        }
    }
}

extern "C" void app_main() {
    #if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
    Serial.begin();
    #endif
    #if ARDUINO_USB_MSC_ON_BOOT && !ARDUINO_USB_MODE
    MSC_Update.begin();
    #endif
    #if ARDUINO_USB_DFU_ON_BOOT && !ARDUINO_USB_MODE
    USB.enableDFU();
    #endif
    #if ARDUINO_USB_ON_BOOT && !ARDUINO_USB_MODE
    USB.begin();
    #endif
    loopTaskWDTEnabled = false;
    initArduino();
    xTaskCreateUniversal(loopTask, "loopTask", getArduinoLoopTaskStackSize(), NULL, 1, &loopTaskHandle, ARDUINO_RUNNING_CORE);
}

#endif

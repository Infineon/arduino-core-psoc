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

  Copyright (c) 2018 Infineon Technologies AG
  This file has been modified for the PSoC microcontroller series.
*/
#ifndef _ARDUINO_H_
#define _ARDUINO_H_

#ifdef __cplusplus
extern "C" {
#endif

// ****************************************************************************
// @Std Includes
// ****************************************************************************
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <types.h>
#include <math.h>
#include <pgmspace.h>

// ****************************************************************************
// @PSoC Lib Includes
// ****************************************************************************


// ****************************************************************************
// @Defines
// ****************************************************************************
#define clockCyclesPerMicrosecond()     (F_CPU / 1000000L)
#define clockCyclesToMicroseconds(a)    (((a) * 1000L) / (F_CPU / 1000L))
#define microsecondsToClockCycles(a)    ((a) * (F_CPU / 1000000L))


// ****************************************************************************
// @Typedefs
// ****************************************************************************

// ****************************************************************************
// @Imported Global Variables
// ****************************************************************************


// ****************************************************************************
// @Prototypes Of Global Functions
// ****************************************************************************

/*
* \brief Arduino yield function.
*/
void yield(void);

/*
* \brief Arduino Main setup function. Called only once at the beginning.
*/
extern void setup(void);

/*
* \brief Arduino Main loop function. Called in an endless loop.
*/
extern void loop(void);


// ****************************************************************************
// @Arduino Core Includes
// ****************************************************************************
#include "wiring_constants.h"
#include "binary.h"
#include "wiring_digital.h"
#include "wiring_analog.h"
#include "wiring_shift.h"
#include "wiring_time.h"
#include "wiring_pulse.h"
#include "itoa.h"
#include "dtostrf.h"
#include "WCharacter.h"
#include "WInterrupts.h"

// ****************************************************************************
// @Infineon Core Includes
// ****************************************************************************
#include "reset.h"

#ifdef __cplusplus
} // extern "C"
#include "Tone.h"
#include "WMath.h"
#endif  // __cplusplus

#include "Print.h"
#include "HardwareSerial.h"

// ****************************************************************************
// @Board Variant Includes
// ****************************************************************************
#include <pins_arduino.h>

#endif  /*_ARDUINO_H_ */

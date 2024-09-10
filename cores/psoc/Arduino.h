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
#include <types.h>
#include <math.h>

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
#include "Binary.h"
#include "itoa.h"
#include "dtostrf.h"

// ****************************************************************************
// @Infineon Core Includes
// ****************************************************************************

#ifdef __cplusplus
} // extern "C"

#endif	// __cplusplus


//****************************************************************************
// @Board Variant Includes
// ****************************************************************************

#endif  /*_ARDUINO_H_ */

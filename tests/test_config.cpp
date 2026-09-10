/**
 * @file test_config.cpp
 * @brief Implementation file for board-specific test configuration variables.
 *
 */

#include "test_config.h"
#include <SPI.h>

#if defined(ARDUINO_ARCH_PSOC6)
// Define the SPI master and slave instances
SPIClassPSOC SPI1 = SPIClassPSOC(PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK, TEST_PIN_SPI_SSEL, true);
#endif // ARDUINO_ARCH_PSOC6

/**
 * @file test_config.h
 * @brief Configuration file for board-specific test pin definitions.
 *
 * This header file contains the definitions of the pins used for testing
 * purposes on the specific board. These pins are configured as output and 
 * input pins for various test scenarios.
 *
 */

#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include <stdint.h>
#include <SPI.h>

// Test Pin Definitions
#if defined(CY8CKIT_062S2_AI)
#define         TEST_PIN_DIGITAL_IO_OUTPUT      7       // IO_4
#define         TEST_PIN_DIGITAL_IO_INPUT       6       // IO_3

#define         TEST_PIN_PULSE                  5       // IO_2

#define         TEST_PIN_SYNC_IO                4       // IO_1

#define         TEST_PIN_SPI_SSEL               3       // IO_0

#define         TEST_PIN_ANALOG_IO_VREF         A0      // Pin connected to VREF
#define         TEST_PIN_ANALOG_IO_DIVIDER      A1      // Pin connected to voltage divider

#define         TEST_ADC_RESOLUTION             11      
#define         TEST_ADC_MAX_VALUE              2048    // 11-bit resolution
#elif defined(CY8CPROTO_063_BLE)
#define         TEST_PIN_DIGITAL_IO_OUTPUT      15
#define         TEST_PIN_DIGITAL_IO_INPUT       14

#define         TEST_PIN_PULSE                  30

#define         TEST_PIN_SYNC_IO                27

#define         TEST_PIN_SPI_SSEL               3

#define         TEST_PIN_ANALOG_IO_VREF         A0      // Pin connected to VREF
#define         TEST_PIN_ANALOG_IO_DIVIDER      A1      // Pin connected to voltage divider

#define         TEST_ADC_RESOLUTION             11      
#define         TEST_ADC_MAX_VALUE              2048    // 11-bit resolution
#endif

#if defined(ARDUINO_ARCH_PSOC6)
// Forward declarations for SPI instances
extern SPIClassPSOC SPI1;
#endif // ARDUINO_ARCH_PSOC6

// Test PWM Frequencies
static const float test_pwm_frequencies[] = {1, 50, 5000, 50000};

#endif // TEST_CONFIG_H
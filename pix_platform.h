#ifndef _PIX_PLATFORM_H
#define _PIX_PLATFORM_H

// Thin compatibility shim so the same sources build both as an Arduino library
// and as a pure ESP-IDF component. Included everywhere the library previously
// pulled in <Arduino.h>.
//
//   defined(ARDUINO)      -> Arduino layer available (arduino-esp32/avr/pico)
//   defined(ESP_PLATFORM) -> ESP32-family target (true under BOTH arduino-esp32
//                            and pure ESP-IDF, since IDF defines it globally)
//   pure ESP-IDF          == defined(ESP_PLATFORM) && !defined(ARDUINO)

#if defined(ARDUINO)

  #include <Arduino.h>

#elif defined(ESP_PLATFORM)

  // Pure ESP-IDF: provide the small Arduino surface the library actually uses.
  #include <cstdint>
  #include <cstring>
  #include <cmath>
  #include <cstdlib>
  #include "esp_timer.h"
  #include "driver/gpio.h"

  #ifndef PROGMEM
    #define PROGMEM
  #endif
  // ESP32 memory is uniformly addressable, so this is a plain dereference -
  // exactly what the arduino-esp32 core's pgmspace shim compiles to.
  #ifndef pgm_read_byte
    #define pgm_read_byte(addr) (*(const uint8_t*)(addr))
  #endif

  #ifndef LOW
    #define LOW 0
  #endif
  #ifndef HIGH
    #define HIGH 1
  #endif
  #ifndef INPUT
    #define INPUT 0
  #endif
  #ifndef OUTPUT
    #define OUTPUT 1
  #endif

  static inline uint32_t micros(void)
  {
    return (uint32_t)esp_timer_get_time();
  }

  static inline void pinMode(uint8_t pin, uint8_t mode)
  {
    gpio_set_direction((gpio_num_t)pin, mode == OUTPUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
  }

  static inline void digitalWrite(uint8_t pin, uint8_t level)
  {
    gpio_set_level((gpio_num_t)pin, level ? 1 : 0);
  }

  #ifndef constrain
    #define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
  #endif

#else

  // Unknown setup - assume an Arduino-style toolchain provides the API.
  #include <Arduino.h>

#endif

// Platforms whose transmission is peripheral-driven (RMT / PIO+DMA) and can
// therefore read the pixels concurrently with the CPU. Only these get the
// Strip<len, bool ASYNC> double-buffer / async API; the bit-bang platforms
// (AVR/ESP8266) transmit with interrupts disabled and cannot be torn, so they
// keep the plain Strip<len>.
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_RP2040)
  #define PIX_STRIP_DOUBLE_BUFFER 1
#endif

#endif // _PIX_PLATFORM_H

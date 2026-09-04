#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)

#include "LED.h"

// ESP8266 has no RMT (ESP32) or PIO/DMA (RP2040) peripheral for this, and the
// Arduino core is single-threaded (NONOS, not FreeRTOS). WS2812B is therefore
// bit-banged, timed against the Xtensa cycle counter (CCOUNT) with interrupts
// disabled for the duration of the frame - the same model as the AVR driver.
// Unlike the I2S/UART tricks this works on any GPIO, matching the library's
// free choice of data pin. Brightness is scaled inline per byte (the fastScale
// from atmega_show.cpp), so no intermediate buffer is allocated.

namespace pix
{
  namespace
  {
    inline uint32_t getCycleCount(void)
    {
      uint32_t ccount;
      __asm__ __volatile__("rsr %0,ccount" : "=a"(ccount));
      return ccount;
    }

    inline uint8_t fastScale(uint8_t c, uint8_t bright)
    {
      return (uint8_t)(((uint16_t)c * bright + c) >> 8);
    }

    uint32_t g_freeShowTimer = 0;

    void transmit(const uint8_t* bytes, uint16_t byteLen, uint8_t pin, uint8_t effBright)
    {
      if (bytes == nullptr || byteLen == 0) return;

      // Cycle counts derived from the live CPU frequency, so this stays correct
      // at both 80 and 160 MHz (and if the frequency is changed at runtime).
      const uint32_t fcpu = (uint32_t)ESP.getCpuFreqMHz() * 1000000UL;
      const uint32_t T0H = fcpu / 2500000; // ~0.40 us high for a 0
      const uint32_t T1H = fcpu / 1250000; // ~0.80 us high for a 1
      const uint32_t PERIOD = fcpu / 800000; // 1.25 us total per bit

      const uint8_t* p = bytes; // GRB(W) bytes, wire order matches the pixel layout
      uint32_t remaining = byteLen;
      const bool pin16 = (pin == 16);
      const uint32_t pinMask = pin16 ? 0u : (1u << pin);

      uint8_t pix = fastScale(*p++, effBright); // effBright == 255 -> identity
      --remaining;
      uint8_t mask = 0x80;
      uint32_t c, start = 0;

      noInterrupts();
      for (;;)
      {
        uint32_t t = (pix & mask) ? T1H : T0H;
        while ((c = getCycleCount()) - start < PERIOD) {} // finish the previous bit's period
        if (pin16) GP16O = 1; else GPOS = pinMask;         // drive high
        start = c;
        while (getCycleCount() - start < t) {}             // hold high
        if (pin16) GP16O = 0; else GPOC = pinMask;         // drive low
        if (!(mask >>= 1))
        {
          if (remaining == 0) break;
          pix = fastScale(*p++, effBright);
          --remaining;
          mask = 0x80;
        }
      }
      while (getCycleCount() - start < PERIOD) {}           // hold the final bit's low time
      interrupts();
    }
  } // namespace

  // Byte-oriented seam: byteLen = pixels * bytes-per-pixel (3 for RGB, 4 for RGBW).
  void _extern_timer_show(const uint8_t* bytes, uint16_t byteLen, uint8_t pin, uint32_t& timer, uint8_t bright)
  {
    while (micros() - timer < 50) {} // WS2812B inter-frame reset/latch gap
    transmit(bytes, byteLen, pin, bright);
    timer = micros();
  }

  void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright, bool scale)
  {
    while (micros() - g_freeShowTimer < 50) {}
    transmit(reinterpret_cast<const uint8_t*>(leds), (uint16_t)(len * 3), pin, scale ? bright : 255);
    g_freeShowTimer = micros();
  }
}

#endif // ESP8266

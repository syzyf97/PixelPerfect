#ifndef _LED_H
#define _LED_H

#include <Arduino.h>

namespace pix {
  
#ifdef AVR
  using range_t = uint16_t; 
#else
  using range_t = uint32_t;
#endif

  struct __attribute__((packed)) LED {
    LED(uint32_t color);
    LED(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);
    uint8_t g;
    uint8_t r;
    uint8_t b;

    operator uint32_t();
    uint8_t& operator[](uint8_t index);
    LED& operator=(uint32_t color);
    LED& operator=(const LED& other);

    bool operator==(const LED& other) const;
    bool operator==(uint32_t color) const;
    bool operator!=(const LED& other) const;
    bool operator!=(uint32_t color) const;
  };

  using Color = LED;

  Color& gamma32(Color& color);
  uint32_t gamma32(uint32_t color);

  void begin(uint8_t pin);

  void clear(LED* leds, range_t len);

  void fill(LED* leds, range_t len, uint32_t color);

  void fill(LED* leds, range_t len, const Color& color);

  void fill(LED* leds, range_t len, uint8_t r, uint8_t g, uint8_t b);

  void fillFromTo(LED* leds, range_t len, uint32_t color, range_t from, range_t to);

  void fillFromTo(LED* leds, range_t len, const Color& color, range_t from, range_t to);

  void fillFromTo(LED* leds, range_t len, uint8_t r, uint8_t g, uint8_t b, range_t from, range_t to);

  Color hsv(uint16_t hue, uint8_t sat = 255u, uint8_t val = 255u);

  Color kelvin(uint16_t temp);

  void show(const LED* leds, range_t len, uint8_t pin, uint8_t bright = 255);
}

#endif // _LED_H

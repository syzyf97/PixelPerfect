#include "LED.h"

#include "gammaTableLUT.h"
#include "kelvinTableLUT.h"

namespace pix
{
  LED::LED(uint32_t color) : g{(uint8_t(color >> 8))}, r{(uint8_t)(color >> 16)}, b{(uint8_t)color} {}

  LED::LED(uint8_t r, uint8_t g, uint8_t b) : g{g}, r{r}, b{b} {}

  LED::LED() : LED(0u) {}

  uint8_t& LED::operator[](uint8_t index)
  {
    static uint8_t void_part = 0u;
    index %= 3;
    switch (index)
    {
      case 0: return r;
      case 1: return g;
      case 2: return b;
      default: return void_part;
    }
    return void_part;
  }

  LED& LED::operator=(const LED& other)
  {
    r = other.r, g = other.g, b = other.b;
    return *this;
  }

  LED& LED::operator=(uint32_t c)
  {
    r = (uint8_t)(c >> 16), g = (uint8_t)(c >> 8), b = (uint8_t)c; 
    return *this;
  }

  bool LED::operator==(const LED& other) const
  {
    return r == other.r && g == other.g && b == other.b;
  }

  bool LED::operator==(uint32_t c) const
  {
    return r == (uint8_t)(c >> 16) && g == (uint8_t)(c >> 8) && b == (uint8_t)c;
  }

  bool LED::operator!=(const LED& other) const
  {
    return r != other.r || g != other.g || b != other.b;
  }

  bool LED::operator!=(uint32_t c) const
  {
    return r != (uint8_t)(c >> 16) || g != (uint8_t)(c >> 8) || b != uint8_t(c);
  }

  LED::operator uint32_t()
  {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }

  __attribute__((weak)) void _begin_periferials(void) {}

  void begin(uint8_t pin)
  {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    _begin_periferials();
  }

  static uint8_t gamma8(uint8_t i)
  {
    return pgm_read_byte(&__GAMMA8_TABLE[i]);
  }

  Color& gamma32(Color& color)
  {
    for (uint8_t i = 0; i < 3; ++i) color[i] = gamma8(color[i]);
    return color;
  }

  uint32_t gamma32(uint32_t color)
  {
    uint8_t* b = (uint8_t*)&color;
    for (uint8_t i = 0; i < 4; ++i) b[i] = gamma8(b[i]);
    return color;
  }

  void fill(LED* leds, uint16_t len, uint32_t color)
  {
    if (leds == nullptr) return;
    for (uint16_t i = 0; i < len; ++i) leds[i] = color;
  }

  void fill(LED* leds, uint16_t len, const Color& color)
  {
    if (leds == nullptr) return;
    for (uint16_t i = 0; i < len; ++i) leds[i] = color;
  }

  void fill(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b)
  {
    if (leds == nullptr) return;
    for (uint16_t i = 0; i < len; ++i) leds[i].r = r, leds[i].g = g, leds[i].b = b;
  }

  void fillFromTo(LED* leds, uint16_t len, uint32_t color, uint16_t from, uint16_t to)
  {
    if (leds == nullptr || from > to || to >= len) return;
    for (uint16_t i = from; i <= to; ++i) leds[i] = color;
  }

  void fillFromTo(LED* leds, uint16_t len, const Color& color, uint16_t from, uint16_t to)
  {
    if (leds == nullptr || from > to || to >= len) return;
    for (uint16_t i = from; i <= to; ++i) leds[i] = color;
  }

  void fillFromTo(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to)
  {
    if (leds == nullptr || from > to || to >= len) return;
    for (uint16_t i = from; i <= to; ++i) leds[i].r = r, leds[i].g = g, leds[i].b = b;
  }

  void clear(LED* leds, uint16_t len)
  {
    if (leds == nullptr) return;
    memset((uint8_t*)leds, 0, len * 3);
  }

  Color hsv(uint16_t hue, uint8_t sat, uint8_t val)
  {
    Color rgb;
    hue = (hue * 1530L + 32768) / 65536L;

    if (hue < 510) { // Red to Green-1
      rgb.b = 0;          // b
      if (hue < 255) { //   Red to Yellow-1
        rgb.r = 255;        // r
        rgb.g = hue;       //     g = 0 to 254
      } else {         //   Yellow to Green-1
        rgb.r = 510 - hue; //     r = 255 to 1
        rgb.g = 255;        // g
      }
    } else if (hue < 1020) { // Green to Blue-1
      rgb.r = 0;            // r
      if (hue < 765) { //   Green to Cyan-1
        rgb.g = 255;        // g
        rgb.b = hue - 510;  //     b = 0 to 254
      } else {          //   Cyan to Blue-1
        rgb.g = 1020 - hue; //     g = 255 to 1
        rgb.b = 255;        // b
      }
    } else if (hue < 1530) { // Blue to Red-1
      rgb.g = 0;            // g
      if (hue < 1275) { //   Blue to Magenta-1
        rgb.r = hue - 1020; //     r = 0 to 254
        rgb.b = 255;        // b
      } else { //   Magenta to Red-1
        rgb.r = 255;        // r
        rgb.b = 1530 - hue; //     b = 255 to 1
      }
    } else { // Last 0.5 Red (quicker than % operator)
      rgb.r = 255;        // r
      rgb.g = rgb.b = 0;      // g = b = 0
    }

    uint32_t v1 = 1 + val;
    uint16_t s1 = 1 + sat;
    uint8_t s2 = 255 - sat;
    
    rgb.r = (((rgb.r * s1) >> 8) + s2) * v1 >> 8;
    rgb.g = (((rgb.g * s1) >> 8) + s2) * v1 >> 8;
    rgb.b = (((rgb.b * s1) >> 8) + s2) * v1 >> 8;

    return rgb;
  }

  Color kelvin(uint16_t temp)
  {
    uint8_t index;
    if (temp <= _COLOR_TEMP_MIN) index = 0;
    else if (temp >= _COLOR_TEMP_MAX) index = _COLOR_TEMP_TABLE_SIZE - 1;
    else index = (uint32_t)(temp - _COLOR_TEMP_MIN) * (_COLOR_TEMP_TABLE_SIZE - 1) / (_COLOR_TEMP_MAX - _COLOR_TEMP_MIN);
    return Color{
      pgm_read_byte(&__COLOR_TEMP_TABLE[index][0]),
      pgm_read_byte(&__COLOR_TEMP_TABLE[index][1]),
      pgm_read_byte(&__COLOR_TEMP_TABLE[index][2]),
    };
  }

}

#ifndef _LED_H
#define _LED_H

#include "pix_platform.h"

namespace pix {

  struct LEDW; // forward declaration for LED::toLEDW()

  struct __attribute__((packed)) LED {
    LED();
    LED(uint32_t color);
    LED(uint8_t r, uint8_t g, uint8_t b);
    uint8_t g;
    uint8_t r;
    uint8_t b;

    // explicit so LED never silently converts to an integer (which would clash
    // with the LEDW overloads and, worse, mis-pack colors). Use (uint32_t)led.
    explicit operator uint32_t() const;        // -> 0xRRGGBB
    uint8_t& operator[](uint8_t index);
    LED& operator=(uint32_t color);
    LED& operator=(const LED& other);

    bool operator==(const LED& other) const;
    bool operator==(uint32_t color) const;
    bool operator!=(const LED& other) const;
    bool operator!=(uint32_t color) const;

    // Explicit RGB -> RGBW promotion (W = 0). The only LED->LEDW conversion;
    // there is deliberately no implicit one.
    LEDW toLEDW() const;
  };

  using Color = LED;

  // 4-channel pixel for RGBW strips (e.g. SK6812). Stored in GRBW wire order.
  // The packed 32-bit form is 0xRRGGBBWW (matches the R,G,B,W index order).
  struct __attribute__((packed)) LEDW {
    LEDW();
    LEDW(uint32_t color);                                  // 0xRRGGBBWW
    LEDW(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);  // 3 args -> W = 0
    uint8_t g;
    uint8_t r;
    uint8_t b;
    uint8_t w;

    explicit operator uint32_t() const;        // -> 0xRRGGBBWW (explicit; see LED)
    uint8_t& operator[](uint8_t index);        // 0=R, 1=G, 2=B, 3=W
    LEDW& operator=(uint32_t color);           // assign from 0xRRGGBBWW
    LEDW& operator=(const LEDW& other);

    bool operator==(const LEDW& other) const;
    bool operator==(uint32_t color) const;
    bool operator!=(const LEDW& other) const;
    bool operator!=(uint32_t color) const;
  };

  using ColorW = LEDW;

  Color& gamma32(Color& color);
  uint32_t gamma32(uint32_t color);

  void begin(uint8_t pin);

  void _begin_periferials(uint8_t pin);

  void clear(LED* leds, uint16_t len);

  void fill(LED* leds, uint16_t len, uint32_t color);

  void fill(LEDW* leds, uint16_t len, uint32_t color_w);

  void fill(LED* leds, uint16_t len, const Color& color);

  void fill(LEDW* leds, uint16_t len, const ColorW& color_w);

  void fill(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b);

  void fill(LEDW* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

  void fillFromTo(LED* leds, uint16_t len, uint32_t color, uint16_t from, uint16_t to);

  void fillFromTo(LEDW* leds, uint16_t len, uint32_t color_w, uint16_t from, uint16_t to);

  void fillFromTo(LED* leds, uint16_t len, const Color& color, uint16_t from, uint16_t to);

  void fillFromTo(LEDW* leds, uint16_t len, const ColorW& color_w, uint16_t from, uint16_t to);

  void fillFromTo(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);

  void fillFromTo(LEDW* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint16_t from, uint16_t to);

  Color hsv(uint16_t hue, uint8_t sat = 255u, uint8_t val = 255u);

  Color kelvin(uint16_t temp);

  // RGBW color helpers: same as hsv()/kelvin() but returning an LEDW (W = 0),
  // so RGBW strips never touch the LED type (no casts, no ambiguity).
  ColorW hsvW(uint16_t hue, uint8_t sat = 255u, uint8_t val = 255u);

  ColorW kelvinW(uint16_t temp);

  void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright = 255, bool scale = true);

  // Copy byteLen pixel bytes from src into dst, scaling each by bright (fast
  // integer approximation, same as the drivers). Used to build a strip's send
  // buffer; byte-oriented so it works for any pixel format (RGB, RGBW).
  void scaleTo(uint8_t* dst, const uint8_t* src, uint16_t byteLen, uint8_t bright);
}

#endif // _LED_H

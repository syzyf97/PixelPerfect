#include "Strip.h"

namespace pix
{
  StripAllocate::StripAllocate() 
  : StripInterface(255), 
    leds{nullptr}, 
    len{0u}
  {}

  StripAllocate::StripAllocate(uint16_t len, uint8_t pin, bool reverse)
  : StripInterface(pin, reverse),
    leds{new LED[len]}, 
    len{len}
  {}

  StripAllocate::~StripAllocate()
  {
    if (leds)
    {
      delete[]leds;
      leds = nullptr;
      len = 0;
    }
  }

  uint16_t StripAllocate::length() const
  {
    return len;
  }

  void StripAllocate::clear(void)
  {
    if (leds)
    {
      pix::clear(leds, len);
    }
  }

  void StripAllocate::show()
  {
    pix::_extern_timer_show(leds, len, pin, timer, brightness);
  }

  void StripAllocate::show(uint8_t bright)
  {
    pix::_extern_timer_show(leds, len, pin, timer, bright);
  }

  void StripAllocate::fill(uint8_t r, uint8_t g, uint8_t b)
  {
    if (leds)
    {
      pix::fill(leds, len, r, g, b);
    }
  }

  void StripAllocate::fill(uint32_t color)
  {
    if (leds)
    {
      pix::fill(leds, len, color);
    }
  }

  void StripAllocate::fill(const Color& color)
  {
    if (leds)
    {
      pix::fill(leds, len, color);
    }
  }

  void StripAllocate::fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to)
  {
    if (leds)
    {
      if (!reverse) return pix::fillFromTo(leds, len, r, g, b, from, to);
      if (from > to || to >= len) return;
      for (uint16_t i = len - 1 - from - to; i < len - from; ++i) leds[i].r = r, leds[i].g = g, leds[i].b = b;
    }
  }

  void StripAllocate::fillFromTo(uint32_t color, uint16_t from, uint16_t to)
  {
    if (leds)
    {
      if (!reverse) return pix::fillFromTo(leds, len, color, from, to);
      if (from > to || to >= len) return;
      for (uint16_t i = len - 1 - from - to; i < len - from; ++i) leds[i] = color;
    }
  }

  void StripAllocate::fillFromTo(const pix::Color& color, uint16_t from, uint16_t to)
  {
    if (leds)
    {
      if (!reverse) return pix::fillFromTo(leds, len, color, from, to);
      if (from > to || to >= len) return;
      for (uint16_t i = len - 1 - from - to; i < len - from; ++i) leds[i] = color;
    }
  }

  LED& StripAllocate::led(uint16_t i)
  {
    static LED voidPixel;
    if (leds && i < len)
    {
      return reverse ? leds[len - 1 - i] : leds[i];
    }
    return voidPixel;
  }

  const LED& StripAllocate::led(uint16_t i) const
  {
    static LED voidPixel;
    if (leds && i < len)
    {
      return reverse ? leds[len - 1 - i] : leds[i];
    }
    return voidPixel;
  }

  LED& StripAllocate::operator[](uint16_t i)
  {
    static LED voidPixel;
    if (leds && i < len)
    {
      return reverse ? leds[len - 1 - i] : leds[i];
    }
    return voidPixel;
  }

  const LED& StripAllocate::operator[](uint16_t i) const
  {
    static LED voidPixel;
    if (leds && i < len)
    {
      return reverse ? leds[len - 1 - i] : leds[i];
    }
    return voidPixel;
  }

  void StripAllocate::changeLength(uint16_t l)
  {
    if (l != len)
    {
      if (leds) delete[]leds;
      leds = new LED[l];
      if (!leds) len = 0u;
      else len = l;
    }
  }
}

#include "Strip.h"

namespace pix
{
  StripAllocate::StripAllocate(range_t _len, uint8_t _pin, bool _reverse)
      : StripInterface{128, reverse},
        pin{pin},
        leds{nullptr},
        timer{millis()}
  {
    if (nullptr != leds)
    {
      delete[] leds;
      leds = nullptr;
    }
    leds = new LED[_len];
    if (nullptr == leds)
      len = 0;
    else
      len = _len;
  }

  StripAllocate::~StripAllocate()
  {
    if (nullptr != leds)
    {
      delete[] leds;
      leds = nullptr;
    }
  }

  bool StripAllocate::updateLength(range_t newLen)
  {
    if (newLen == len)
      return true;
    if (nullptr != leds)
    {
      delete[] leds;
      leds = nullptr;
    }
    leds = new LED[newLen];
    if (nullptr == leds)
    {
      len = 0;
      return false;
    }
    len = newLen;
    return true;
  }

  void StripAllocate::begin()
  {
    pix::begin(pin);
  }

  void StripAllocate::clear()
  {
    pix::clear(leds, len);
  }

  LED &StripAllocate::operator[](range_t idx)
  {
    return led(idx);
  }

  const LED &StripAllocate::operator[](range_t idx) const
  {
    return led(idx);
  }

  LED &StripAllocate::led(range_t idx)
  {
    if (nullptr == leds || idx >= len)
      return voidLED;
    if (reverse)
      return leds[len - 1 - idx];
    return leds[idx];
  }

  const LED &StripAllocate::led(range_t idx) const
  {
    if (nullptr == leds || idx >= len)
      return voidLED;
    if (reverse)
      return leds[len - 1 - idx];
    return leds[idx];
  }

  range_t StripAllocate::length() const
  {
    return len;
  }

  void StripAllocate::fill(uint8_t r, uint8_t g, uint8_t b)
  {
    pix::fill(leds, len, r, g, b);
  }

  void StripAllocate::fill(uint32_t color)
  {
    pix::fill(leds, len, color);
  }

  void StripAllocate::fill(const Color &color)
  {
    pix::fill(leds, len, color);
  }

  void StripAllocate::fillFromTo(uint8_t r, uint8_t g, uint8_t b, range_t from, range_t to)
  {
    if (!reverse)
      pix::fillFromTo(leds, len, r, g, b, from, to);
    if (from > to || to >= len)
      return;
    for (range_t i = len - 1 - from - to; i < len - from; ++i)
      leds[i] = {r, g, b};
  }

  void StripAllocate::fillFromTo(uint32_t color, range_t from, range_t to)
  {
    if (!reverse)
      pix::fillFromTo(leds, len, color, from, to);
    if (from > to || to >= len)
      return;
    for (range_t i = len - 1 - from - to; i < len - from; ++i)
      leds[i] = color;
  }

  void StripAllocate::fillFromTo(const Color &color, range_t from, range_t to)
  {
    if (!reverse)
      pix::fillFromTo(leds, len, color, from, to);
    if (from > to || to >= len)
      return;
    for (range_t i = len - 1 - from - to; i < len - from; ++i)
      leds[i] = color;
  }

  void StripAllocate::show(uint8_t tempBrightness)
  {
    pix::_extern_timer_show(leds, len, pin, timer, tempBrightness);
  }
}
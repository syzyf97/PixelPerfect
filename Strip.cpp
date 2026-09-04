#include "Strip.h"

namespace pix
{
  template<class PixelT>
  BasicStripAllocate<PixelT>::BasicStripAllocate()
  : BasicStripInterface<PixelT>(255), leds{nullptr}, len{0u}
  {}

  template<class PixelT>
  BasicStripAllocate<PixelT>::BasicStripAllocate(uint16_t len, uint8_t pin, bool reverse)
  : BasicStripInterface<PixelT>(pin, reverse), leds{new PixelT[len]}, len{len}
  {}

  template<class PixelT>
  BasicStripAllocate<PixelT>::~BasicStripAllocate()
  {
    if (leds)
    {
      delete[] leds;
      leds = nullptr;
      len = 0;
    }
  }

  template<class PixelT>
  uint16_t BasicStripAllocate<PixelT>::length() const { return len; }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::clear(void)
  {
    if (leds) memset((void*)leds, 0, len * sizeof(PixelT));
  }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::show(void)
  {
    pix::_extern_timer_show((const uint8_t*)leds, (uint16_t)(len * sizeof(PixelT)), this->pin, this->timer, this->brightness);
  }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::show(uint8_t bright)
  {
    pix::_extern_timer_show((const uint8_t*)leds, (uint16_t)(len * sizeof(PixelT)), this->pin, this->timer, bright);
  }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::fill(uint8_t r, uint8_t g, uint8_t b)
  {
    if (leds) for (uint16_t i = 0; i < len; ++i) leds[i] = PixelT(r, g, b);
  }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::fill(uint32_t color)
  {
    if (leds) for (uint16_t i = 0; i < len; ++i) leds[i] = color;
  }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::fill(const PixelT& color)
  {
    if (leds) for (uint16_t i = 0; i < len; ++i) leds[i] = color;
  }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to)
  {
    fillFromTo(PixelT(r, g, b), from, to);
  }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::fillFromTo(uint32_t color, uint16_t from, uint16_t to)
  {
    fillFromTo(PixelT(color), from, to);
  }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::fillFromTo(const PixelT& color, uint16_t from, uint16_t to)
  {
    if (!leds || from > to || to >= len) return;
    if (!this->reverse) { for (uint16_t i = from; i <= to; ++i) leds[i] = color; }
    else { for (uint16_t i = from; i <= to; ++i) leds[len - 1 - i] = color; }
  }

  template<class PixelT>
  PixelT& BasicStripAllocate<PixelT>::led(uint16_t i)
  {
    static PixelT voidPixel;
    if (leds && i < len) return this->reverse ? leds[len - 1 - i] : leds[i];
    return voidPixel;
  }

  template<class PixelT>
  const PixelT& BasicStripAllocate<PixelT>::led(uint16_t i) const
  {
    static PixelT voidPixel;
    if (leds && i < len) return this->reverse ? leds[len - 1 - i] : leds[i];
    return voidPixel;
  }

  template<class PixelT>
  PixelT& BasicStripAllocate<PixelT>::operator[](uint16_t i) { return led(i); }

  template<class PixelT>
  const PixelT& BasicStripAllocate<PixelT>::operator[](uint16_t i) const { return led(i); }

  template<class PixelT>
  void BasicStripAllocate<PixelT>::changeLength(uint16_t l)
  {
    if (l != len)
    {
      if (leds) delete[] leds;
      leds = new PixelT[l];
      len = leds ? l : 0u;
    }
  }

  // Explicit instantiations (one per supported pixel format).
  template class BasicStripAllocate<LED>;
  template class BasicStripAllocate<LEDW>;
}

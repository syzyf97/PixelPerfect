#ifndef _STRIP_H
#define _STRIP_H

#include "LED.h"

namespace pix
{

  extern void _extern_timer_show(const LED* leds, range_t len, uint8_t pin, uint32_t& timer, uint8_t bright);

  static LED voidLED;

  class StripInterface 
  {
  protected:
    StripInterface(uint8_t _b = 128, bool _r = false) : brightness{_b}, reverse{_r} {}
  public:
    virtual ~StripInterface() = default;
    uint8_t brightness;
    bool reverse;
    virtual void begin() = 0;
    virtual void show() { show(brightness); }
    virtual void show(uint8_t tempBrightness) = 0;
    virtual void clear() = 0;
    virtual range_t length() const = 0;
    virtual LED& led(range_t idx) = 0;
    virtual const LED& led(range_t idx) const = 0;
    virtual void fill(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void fill(uint32_t color) = 0;
    virtual void fill(const LED& color) = 0;
    virtual void fillFromTo(uint8_t r, uint8_t g, uint8_t b, range_t from, range_t to) = 0;
    virtual void fillFromTo(uint32_t color, range_t from, range_t to) = 0;
    virtual void fillFromTo(const LED& color, range_t from, range_t to) = 0;
  };

  using StripHandler = StripInterface*;

  template<range_t PIX_COUNT>
  class StripTemplate final : public StripInterface 
  {
  public:
    StripTemplate(uint8_t _pin = 255, bool _reverse = false)
    : StripInterface{128, _reverse}
      pin{_pin},
      timer{millis()}
    {}
    ~StripTemplate() {}

    void begin() override 
    {
      pix::begin(pin);
    }

    void clear() override 
    {
      pix::clear(leds, PIX_COUNT);
    }

    void show(uint8_t tempBrightness) override 
    {
      _extern_timer_show(leds, PIX_COUNT, pin, timer, tempBrightness);
    }

    range_t length() const override { return PIX_COUNT; }

    void fill(uint8_t r, uint8_t g, uint8_t b) override 
    {
      for (range_t i = 0; i < PIX_COUNT; ++i) leds[i] = {r, g, b};
    }

    LED& led(range_t idx) override 
    {
      if (idx >= PIX_COUNT) return voidLED;
      if (reverse) return leds[PIX_COUNT - 1 - idx];
      return leds[idx];
    }

    LED& operator[](range_t idx)
    {
      return led(idx);
    }

    const LED& led(range_t idx) const override
    {
      if (idx >= PIX_COUNT) return voidLED;
      if (reverse) return leds[PIX_COUNT - 1 - idx];
      return leds[idx];
    }

    const LED& operator[](range_t idx) const
    {
      return led(idx);
    }

    void fill(uint32_t color) override
    {
      fill((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
    }

    void fill(const LED& color) override
    {
      fill(color.r, color.g, color.b);
    }

    void fillFromTo(uint8_t r, uint8_t g, uint8_t b, range_t from, range_t to) override
    {
      if (!reverse) return pix::fillFromTo(leds, PIX_COUNT, r, g, b, from, to);
      if (from > to || to >= PIX_COUNT) return;
      for (range_t i = PIX_COUNT - 1 - from - to; i < PIX_COUNT - from; ++i) leds[i].r = r, leds[i].g = g, leds[i].b = b;
    }

    void fillFromTo(uint32_t color, range_t from, range_t to) override
    {
      if (!reverse) return pix::fillFromTo(leds, PIX_COUNT, color, from, to);
      if (from > to || to >= PIX_COUNT) return;
      for (range_t i = PIX_COUNT - 1 - from - to; i < PIX_COUNT - from; ++i) leds[i] = color;
    }

    void fillFromTo(const Color& color, range_t from, range_t to) override
    {
      if (!reverse) return pix::fillFromTo(leds, PIX_COUNT, color, from, to);
      if (from > to || to >= PIX_COUNT) return;
      for (range_t i = PIX_COUNT - 1 - from - to; i < PIX_COUNT - from; ++i) leds[i] = color;
    }

  private:
    uint8_t pin;
    uint32_t timer;
    LED leds[PIX_COUNT];
  };

  template<range_t S>
  using Strip = StripTemplate<S>;


  class StripAllocate final : public StripInterface
  {
  public:
    StripAllocate(range_t _len = 0, uint8_t _pin = 255, bool _reverse = false);
    ~StripAllocate();
    bool updateLength(range_t newLen);
    LED& operator[](range_t idx);
    const LED& operator[](range_t idx) const;
    LED& led(range_t idx) override;
    const LED& led(range_t idx) const override;
    void begin() override;
    void clear() override;
    range_t length() const override;
    void fill(uint8_t r, uint8_t g, uint8_t b) override;
    void fill(uint32_t color) override;
    void fill(const Color& color) override;
    void fillFromTo(uint8_t r, uint8_t g, uint8_t b, range_t from, range_t to) override;
    void fillFromTo(uint32_t color, range_t from, range_t to) override;
    void fillFromTo(const Color& color, range_t from, range_t to) override;
    void show(uint8_t tempBrightness) override;

  private:
    range_t len;
    LED* leds;
    uint8_t pin;
    uint32_t timer;
  };

}


#endif // _STRIP_H

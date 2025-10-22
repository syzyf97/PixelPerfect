#ifndef _STRIP_H
#define _STRIP_H

#include "LED.h"

namespace pix
{

  class StripInterface
  {
  public:
    StripInterface(uint8_t pin, bool reverse = false)
    : brightness{120}, pin{pin}, reverse{reverse}, timer{0ul} {}
    virtual ~StripInterface() = default;
    uint8_t brightness;
    virtual uint16_t length() const = 0;
    virtual void clear(void) = 0;
    void begin(void) { pix::begin(pin); }
    virtual void show(void) = 0;
    virtual void show(uint8_t bright) = 0;
    virtual void fill(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void fill(uint32_t color) = 0;
    virtual void fill(const Color& color) = 0;
    virtual void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to) = 0;
    virtual void fillFromTo(uint32_t color, uint16_t from, uint16_t to) = 0;
    virtual void fillFromTo(const Color& color, uint16_t from, uint16_t to) = 0;
    virtual LED& led(uint16_t i) = 0;
    virtual const LED& led(uint16_t i) const = 0;
    inline void setPin(uint8_t p) { pin = p; }
    inline uint8_t getPin(void) const { return pin; }
    inline bool isReverse(void) const { return reverse; }
    inline void setReverse(bool r) { reverse = r; }
    
  protected:
    uint8_t pin;
    bool reverse;
    uint32_t timer;
  };

  extern void _extern_timer_show(const LED* leds, uint16_t len, uint8_t pin, uint32_t& timer, uint8_t bright = 255);

  template<uint16_t len>
  class StripTemplate final : public StripInterface
  {
  public:
    StripTemplate() : StripInterface(255) {}
    StripTemplate(uint8_t pin, bool reverse = false)
    : StripInterface{pin, reverse} {}
    uint16_t length() const override final { return len; }
  
    ~StripTemplate() override {}
  
    void clear(void) override
    {
      pix::clear(leds, len);
    }

    void show(void) override
    {
      pix::_extern_timer_show(leds, len, pin, timer, brightness);
    }

    void show(uint8_t bright) override
    {
      pix::_extern_timer_show(leds, len, pin, timer, bright);
    }

    void fill(uint8_t r, uint8_t g, uint8_t b) override
    {
      pix::fill(leds, len, r, g, b);
    }
  
    void fill(uint32_t color) override
    {
      pix::fill(leds, len, color);
    }
  
    void fill(const pix::Color& color) override
    {
      pix::fill(leds, len, color);
    }
  
    void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to) override
    {
      if (!reverse) return pix::fillFromTo(leds, len, r, g, b, from, to);
      if (from > to || to >= len) return;
      for (uint16_t i = len - 1 - from - to; i < len - from; ++i) leds[i].r = r, leds[i].g = g, leds[i].b = b;
    }
  
    void fillFromTo(uint32_t color, uint16_t from, uint16_t to) override
    {
      if (!reverse) return pix::fillFromTo(leds, len, color, from, to);
      if (from > to || to >= len) return;
      for (uint16_t i = len - 1 - from - to; i < len - from; ++i) leds[i] = color;
    }
  
    void fillFromTo(const pix::Color& color, uint16_t from, uint16_t to) override
    {
      if (!reverse) return pix::fillFromTo(leds, len, color, from, to);
      if (from > to || to >= len) return;
      for (uint16_t i = len - 1 - from - to; i < len - from; ++i) leds[i] = color;
    }
    
    LED& led(uint16_t i) override
    {
      static pix::LED voidPixel;
      if (i < len)
      {
        return reverse ? leds[len - 1 - i] : leds[i];
      }
      return voidPixel;
    }
    
    const LED& led(uint16_t i) const override
    {
      static pix::LED voidPixel;
      if (i < len)
      {
        return reverse ? leds[len - 1 - i] : leds[i];
      }
      return voidPixel;
    }
    
    LED& operator[](uint16_t i)
    {
      static pix::LED voidPixel;
      if (i < len)
      {
        return reverse ? leds[len - 1 - i] : leds[i];
      }
      return voidPixel;
    }
    const LED& operator[](uint16_t i) const
    {
      static pix::LED voidPixel;
      if (i < len)
      {
        return reverse ? leds[len - 1 - i] : leds[i];
      }
      return voidPixel;
    }
  private:
    pix::LED leds[len];
  };
  
  class StripAllocate final : public StripInterface
  {
  public:
    StripAllocate();
    StripAllocate(uint16_t pixelCount, uint8_t pin, bool reverse = false);
    ~StripAllocate() override;
    uint16_t length() const override;
    void clear(void) override;
    void show(void) override;
    void show(uint8_t bright) override;
    void fill(uint8_t r, uint8_t g, uint8_t b) override;
    void fill(uint32_t color) override;
    void fill(const Color& color) override;
    void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to) override;
    void fillFromTo(uint32_t color, uint16_t from, uint16_t to) override;
    void fillFromTo(const Color& color, uint16_t from, uint16_t to) override;
    LED& led(uint16_t i) override;
    const LED& led(uint16_t i) const override;
    LED& operator[](uint16_t i);
    const LED& operator[](uint16_t i) const;
    void changeLength(uint16_t l);
  
  private:
    pix::LED* leds;
    uint16_t len;
  };
  
  using StripHandler = StripInterface*;
  
  template<uint16_t S>
  using Strip = StripTemplate<S>;
}


#endif // _STRIP_H

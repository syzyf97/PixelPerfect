#ifndef _STRIP_GROUP_H
#define _STRIP_GROUP_H

#include "Strip.h"

namespace pix
{
  class StripGroupInterface
  {
  public:
    StripGroupInterface() : brightness{128} {}
    virtual ~StripGroupInterface() = default;
    uint8_t brightness;
    virtual uint32_t length() const = 0;
    virtual void begin(void) = 0;
    virtual void clear(void) = 0;
    virtual void show(void) = 0;
    virtual void show(uint8_t bright) = 0;
    virtual void showStrip(uint8_t strip) = 0;
    virtual void fill(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void fill(uint32_t color) = 0;
    virtual void fill(const Color& color) = 0;
    virtual void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to) = 0;
    virtual void fillFromTo(uint32_t color, uint32_t from, uint32_t to) = 0;
    virtual void fillFromTo(const Color& color, uint32_t from, uint32_t to) = 0;
    virtual LED& led(uint32_t i) = 0;
    virtual const LED& led(uint32_t i) const = 0;
    virtual StripHandler strip(uint8_t s) = 0;
    virtual const StripHandler strip(uint8_t s) const = 0;
  };

  using StripGroupHandler = StripGroupInterface*;

  template<uint8_t STRIP_COUNT>
  class StripGroup final : public StripGroupInterface
  {
  public:
    StripGroup() : StripGroupInterface() {};
    template<uint8_t _LOCAL_STRIP_COUNT>
    StripGroup(const StripHandler (&s)[_LOCAL_STRIP_COUNT]) : StripGroupInterface()
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (i < _LOCAL_STRIP_COUNT) strips[i] = s[i];
        else strips[i] = nullptr;
      }
    }
    ~StripGroup() override {}

    uint32_t length() const override
    {
      uint32_t l = 0ul;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (strips[i]) l += strips[i]->length();
      }
      return l;
    }

    void begin(void) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (strips[i]) strips[i]->init();
      }
    }

    void clear(void) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (strips[i]) strips[i]->clear();
      }
    }

    void show(void) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (strips[i]) strips[i]->show(brightness);
      }
    }

    void show(uint8_t bright) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (strips[i]) strips[i]->show(bright);
      }
    }

    void showStrip(uint8_t strip) override
    {
      if (strip < STRIP_COUNT && strips[strip]) strips[strip]->show(brightness);
    }

    void fill(uint8_t r, uint8_t g, uint8_t b) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (strips[i]) strips[i]->fill(r, g, b);
      }
    }

    void fill(uint32_t color) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (strips[i]) strips[i]->fill(color);
      }
    }

    void fill(const Color& color) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (strips[i]) strips[i]->fill(color);
      }
    }

    void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to) override
    {
      if (from > to || to >= length()) return;
      for (uint32_t i = from; i <= to; ++i)
      {
        led(i) = {r, g, b};
      }
    }

    void fillFromTo(uint32_t color, uint32_t from, uint32_t to) override
    {
      if (from > to || to >= length()) return;
      for (uint32_t i = from; i <= to; ++i)
      {
        led(i) = color;
      }
    }

    void fillFromTo(const Color& color, uint32_t from, uint32_t to) override
    {
      if (from > to || to >= length()) return;
      for (uint32_t i = from; i <= to; ++i)
      {
        led(i) = color;
      }
    }

    LED& led(uint32_t n) override
    {
      static LED voidPixel;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (!strips[i]) continue;
        if (n < strips[i]->length())
        {
          return strips[i]->led(n);
        }
        n -= strips[i]->length();
      }
      return voidPixel;
    }

    const LED& led(uint32_t n) const override
    {
      static LED voidPixel;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (!strips[i]) continue;
        if (n < strips[i]->length())
        {
          return strips[i]->led(n);
        }
        n -= strips[i]->length();
      }
      return voidPixel;
    }

    StripHandler strip(uint8_t s) override
    {
      if (s < STRIP_COUNT) return strips[s];
      return nullptr;
    }

    const StripHandler strip(uint8_t s) const override
    {
      if (s < STRIP_COUNT) return strips[s];
      return nullptr;
    }

    LED& operator[](uint32_t n)
    {
      static LED voidPixel;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (!strips[i]) continue;
        if (n < strips[i]->length())
        {
          return strips[i]->led(n);
        }
        n -= strips[i]->length();
      }
      return voidPixel;
    }

    const LED& operator[](uint32_t n) const
    {
      static LED voidPixel;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (!strips[i]) continue;
        if (n < strips[i]->length())
        {
          return strips[i]->led(n);
        }
        n -= strips[i]->length();
      }
      return voidPixel;
    }

  private:
    StripHandler strips[STRIP_COUNT];
  };
}


#endif // _STRIP_GROUP_H

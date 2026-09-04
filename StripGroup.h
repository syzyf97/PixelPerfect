#ifndef _STRIP_GROUP_H
#define _STRIP_GROUP_H

#include "Strip.h"

namespace pix
{
  // Aggregates several same-format strips behind one global pixel index space.
  // Templated on the pixel format, so a group can only hold strips of that
  // format (mixing RGB and RGBW in one group is a compile error, by design).
  template<class PixelT>
  class BasicStripGroupInterface
  {
  public:
    BasicStripGroupInterface() : brightness{128} {}
    virtual ~BasicStripGroupInterface() = default;
    uint8_t brightness;
    virtual uint32_t length() const = 0;
    virtual uint32_t recalcLength() = 0;
    virtual void begin(void) = 0;
    virtual void clear(void) = 0;
    virtual void show(void) = 0;
    virtual void show(uint8_t bright) = 0;
    virtual void showStrip(uint8_t strip) = 0;
    virtual void fill(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void fill(uint32_t color) = 0;
    virtual void fill(const PixelT& color) = 0;
    virtual void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to) = 0;
    virtual void fillFromTo(uint32_t color, uint32_t from, uint32_t to) = 0;
    virtual void fillFromTo(const PixelT& color, uint32_t from, uint32_t to) = 0;
    virtual PixelT& led(uint32_t i) = 0;
    virtual const PixelT& led(uint32_t i) const = 0;
    virtual BasicStripInterface<PixelT>* strip(uint8_t s) = 0;
    virtual BasicStripInterface<PixelT>* strip(uint8_t s) const = 0;
  };

  template<uint8_t STRIP_COUNT, class PixelT>
  class BasicStripGroup final : public BasicStripGroupInterface<PixelT>
  {
    typedef BasicStripInterface<PixelT>* Handle;
    typedef BasicStripGroupInterface<PixelT> Base;
  public:
    BasicStripGroup() : Base() {}
    BasicStripGroup(const Handle (&s)[STRIP_COUNT]) : Base()
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i) strips[i] = s[i];
    }
    ~BasicStripGroup() override {}

    uint32_t length() const override { return len; }

    uint32_t recalcLength(void) override
    {
      len = 0UL;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        if (strips[i]) len += strips[i]->length();
      return len;
    }

    void begin(void) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        if (strips[i]) strips[i]->begin();
    }

    void clear(void) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        if (strips[i]) strips[i]->clear();
    }

    void show(void) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        if (strips[i]) strips[i]->show(this->brightness);
    }

    void show(uint8_t bright) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        if (strips[i]) strips[i]->show(bright);
    }

    void showStrip(uint8_t strip) override
    {
      if (strip < STRIP_COUNT && strips[strip]) strips[strip]->show(this->brightness);
    }

    void fill(uint8_t r, uint8_t g, uint8_t b) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        if (strips[i]) strips[i]->fill(r, g, b);
    }

    void fill(uint32_t color) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        if (strips[i]) strips[i]->fill(color);
    }

    void fill(const PixelT& color) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        if (strips[i]) strips[i]->fill(color);
    }

    void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to) override
    {
      fillFromTo(PixelT(r, g, b), from, to);
    }

    void fillFromTo(uint32_t color, uint32_t from, uint32_t to) override
    {
      fillFromTo(PixelT(color), from, to);
    }

    void fillFromTo(const PixelT& color, uint32_t from, uint32_t to) override
    {
      if (from > to || to >= length()) return;
      for (uint32_t i = from; i <= to; ++i) led(i) = color;
    }

    PixelT& led(uint32_t n) override { return at(n); }
    const PixelT& led(uint32_t n) const override { return at(n); }
    PixelT& operator[](uint32_t n) { return at(n); }
    const PixelT& operator[](uint32_t n) const { return at(n); }

    Handle strip(uint8_t s) override { return s < STRIP_COUNT ? strips[s] : nullptr; }
    Handle strip(uint8_t s) const override { return s < STRIP_COUNT ? strips[s] : nullptr; }

  private:
    PixelT& at(uint32_t n)
    {
      static PixelT voidPixel;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (!strips[i]) continue;
        if (n < strips[i]->length()) return strips[i]->led(n);
        n -= strips[i]->length();
      }
      return voidPixel;
    }
    const PixelT& at(uint32_t n) const
    {
      static PixelT voidPixel;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (!strips[i]) continue;
        if (n < strips[i]->length()) return strips[i]->led(n);
        n -= strips[i]->length();
      }
      return voidPixel;
    }

    Handle strips[STRIP_COUNT];
    uint32_t len;
  };

  // ---- RGB (LED) facade ----------------------------------------------------
  using StripGroupInterface = BasicStripGroupInterface<LED>;
  using StripGroupHandler = StripGroupInterface*;

  template<uint8_t N>
  using StripGroup = BasicStripGroup<N, LED>;

  // ---- RGBW (LEDW) facade --------------------------------------------------
  using WStripGroupInterface = BasicStripGroupInterface<LEDW>;
  using WStripGroupHandler = WStripGroupInterface*;

  template<uint8_t N>
  using WStripGroup = BasicStripGroup<N, LEDW>;
}


#endif // _STRIP_GROUP_H

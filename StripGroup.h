#ifndef _STRIP_GROUP_H
#define _STRIP_GROUP_H

#include "Strip.h"

namespace pix
{
  template<uint8_t STRIP_COUNT>
  class StripGroupInterface
  {
  public:
    ~StripGroupInterface() = default;
    virtual range_t recalcLength() = 0;
    virtual StripHandler strip(uint8_t stripIdx) = 0;
    virtual const StripHandler strip(uint8_t stripIdx) const = 0;
    virtual StripHandler stripByLED(range_t ledIdx) = 0;
    virtual const StripHandler stripByLED(range_t ledIdx) const = 0;
    virtual void showStrip(uint8_t stripIdx, uint8_t tempBrightness) = 0;
    void showStrip(uint8_t stripIdx) { showStrip(stripIdx, brightness); }
    virtual void reconfigStrips(StripHandler (&s)[STRIP_COUNT]) = 0;
  };

  template <uint8_t STRIP_COUNT>
  class StripGroup final : public StripInterface, public StripGroupInterface<STRIP_COUNT>
  {
  public:
    StripGroup(bool _reverse = false) : StripInterface{128, _reverse}, len{0}
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i) strips[i] = nullptr;
    }

    StripGroup(StripHandler (&s)[STRIP_COUNT])
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        strips[i] = s[i];
      }
    }

    void begin() override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (nullptr != strips[i])
          strips[i]->begin();
      }
    }

    void clear() override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (nullptr != strips[i])
          strips[i]->clear();
      }
    }

    void show(uint8_t tempBrightness) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (nullptr != strips[i])
          strips[i]->show(tempBrightness);
      }
    }

    void showStrip(uint8_t stripIdx, uint8_t tempBrightness) override
    {
      if (stripIdx >= STRIP_COUNT) return;
      if (nullptr == strips[stripIdx]) return;
      strips[stripIdx]->show(tempBrightness);
    }

    range_t length() const override
    {
      return len;
    }

    range_t recalcLength() override
    {
      len = 0;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        len += (nullptr == strips[i]) ? 0 : strips[i]->length();
      }
      return len;
    }

    StripHandler strip(uint8_t stripIdx) override
    {
      if (stripIdx >= STRIP_COUNT) return nullptr;
      if (!reverse)
      {
        return strips[stripIdx];
      }
      else
      {
        return strips[STRIP_COUNT - 1 - stripIdx];
      }
    }

    const StripHandler strip(uint8_t stripIdx) const override
    {
      if (stripIdx >= STRIP_COUNT) return nullptr;
      if (!reverse)
      {
        return strips[stripIdx];
      }
      else
      {
        return strips[STRIP_COUNT - 1 - stripIdx];
      }
    }

    StripHandler stripByLED(range_t ledIdx) override
    {
      if (ledIdx >= len) return nullptr;
      if (reverse)
      {
        for (uint8_t i = STRIP_COUNT - 1; i-- > 0;)
        {
          if (nullptr == strips[i]) continue;
          if (strips[i]->length() > ledIdx)
            return strips[i];
          ledIdx -= strips[i]->length();
        }
      }
      else
      {
        for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        {
          if (nullptr == strips[i]) continue;
          if (strips[i]->length() > ledIdx)
            return strips[i];
          ledIdx -= strips[i]->length();
        }
      }
      return nullptr;
    }

    const StripHandler stripByLED(range_t ledIdx) const override
    {
      if (ledIdx >= len) return nullptr;
      if (reverse)
      {
        for (uint8_t i = STRIP_COUNT - 1; i-- > 0;)
        {
          if (nullptr == strips[i]) continue;
          if (strips[i]->length() > ledIdx)
            return strips[i];
          ledIdx -= strips[i]->length();
        }
      }
      else
      {
        for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        {
          if (nullptr == strips[i]) continue;
          if (strips[i]->length() > ledIdx)
            return strips[i];
          ledIdx -= strips[i]->length();
        }
      }
      return nullptr;
    }

    LED& led(range_t ledIdx) override
    {
      if (ledIdx >= len) return voidLED;
      if (reverse)
      {
        for (uint8_t i = STRIP_COUNT - 1; i-- > 0;)
        {
          if (nullptr == strips[i]) continue;
          if (strips[i]->length() > ledIdx)
            return strips[i]->led(strips[i]->length() - 1 - ledIdx);
          ledIdx -= strips[i]->length();
        }
      }
      else
      {
        for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        {
          if (nullptr == strips[i]) continue;
          if (strips[i]->length() > ledIdx)
            return strips[i]->led(ledIdx);
          ledIdx -= strips[i]->length();
        }
      }
      return voidLED;
    }

    const LED& led(range_t ledIdx) const override
    {
      if (ledIdx >= len) return voidLED;
      if (reverse)
      {
        for (uint8_t i = STRIP_COUNT - 1; i-- > 0;)
        {
          if (nullptr == strips[i]) continue;
          if (strips[i]->length() > ledIdx)
            return strips[i]->led(strips[i]->length() - 1 - ledIdx);
          ledIdx -= strips[i]->length();
        }
      }
      else
      {
        for (uint8_t i = 0; i < STRIP_COUNT; ++i)
        {
          if (nullptr == strips[i]) continue;
          if (strips[i]->length() > ledIdx)
            return strips[i]->led(ledIdx);
          ledIdx -= strips[i]->length();
        }
      }
      return voidLED;
    }

    void fill(uint8_t r, uint8_t g, uint8_t b) override
    {
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (nullptr != strips[i]) strips[i]->fill(r, g, b);
      }
    }

    void fill(uint32_t color) override
    {
      return this->fill((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
    }

    void fill(const Color& color) override
    {
      return this->fill(color.r, color.g, color.b);
    }

    void fillFromTo(uint8_t r, uint8_t g, uint8_t b, range_t from, range_t to) override
    {
      if (from > to || to >= len) return;
      if (reverse)
      {
        range_t tfrom = len - 1 - to;
        to = len - 1 - from;
        from = tfrom;
      }

      range_t globIdx = 0;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        if (nullptr == strips[i]) continue;
        range_t stripLen = strips[i]->length();
        range_t stripStart = globIdx;
        range_t stripEnd = globIdx + stripLen - 1;
        if (to >= stripStart && from <= stripEnd)
        {
          range_t localFrom = (from > stripStart) ? (from - stripStart) : 0;
          range_t localTo = (to < stripEnd) ? (to - stripStart) : (stripLen - 1);
          strips[i]->fillFromTo(r, g, b, localFrom, localTo);
        }
        globIdx += stripLen;
      }
    }

    void fillFromTo(uint32_t color, range_t from, range_t to) override
    {
      return this->fillFromTo((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, from, to);
    }

    void fillFromTo(const Color& color, range_t from, range_t to) override
    {
      return this->fillFromTo(color.r, color.g, color.b, from, to);
    }

    void reconfigStrips(StripHandler (&s)[STRIP_COUNT]) override
    {
      len = 0;
      for (uint8_t i = 0; i < STRIP_COUNT; ++i)
      {
        strips[i] = s[i];
        if (nullptr != strips[i]) len += strips[i]->length();
      }
    }
    private:
    range_t len;
    StripHandler strips[STRIP_COUNT];
  };
}
#endif // _STRIP_GROUP_H

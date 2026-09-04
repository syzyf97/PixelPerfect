#ifndef _STRIP_H
#define _STRIP_H

#include "LED.h"

namespace pix
{
  // Byte-oriented transmission seam. byteLen = pixels * bytes-per-pixel
  // (3 for RGB / LED, 4 for RGBW / LEDW); the driver scales each byte by bright.
  extern void _extern_timer_show(const uint8_t* bytes, uint16_t byteLen, uint8_t pin, uint32_t& timer, uint8_t bright = 255);

  // True if `pin` currently has (or can currently get) a hardware channel.
  // AVR/ESP8266 (no channel concept) always return true (weak default in
  // LED.cpp). On RP2040/ESP32 with the pooled+queued driver, a transient
  // false just means every channel is busy right now - the next show() will
  // still succeed, only queued. On ESP32 with the new (IDF5+) RMT driver,
  // which cannot pool, false is durable once more pins are live than there
  // are physical channels - see the "Channel pooling" section in README.
  extern bool _extern_channel_ok(uint8_t pin);

#if PIX_STRIP_DOUBLE_BUFFER
  // Non-blocking seam (RMT / PIO+DMA); `bytes` is a pre-scaled, caller-owned
  // buffer that must stay valid until the transfer finishes.
  extern void _extern_show_async_begin(const uint8_t* bytes, uint16_t byteLen, uint8_t pin, uint32_t& timer);
  extern void _extern_show_async_wait(uint8_t pin, uint32_t& timer);
  extern bool _extern_show_busy(uint8_t pin);

  template<bool B> struct AsyncTag {}; // C++11-safe dispatch tag (no if constexpr)

  // Send buffer present only when ASYNC is enabled (empty otherwise).
  template<uint16_t N, class PixelT, bool A> struct SendBuf { uint8_t data[N * sizeof(PixelT)]; };
  template<uint16_t N, class PixelT> struct SendBuf<N, PixelT, false> {};
#endif

  // Format-agnostic + pixel-typed interface, shared by every color format.
  // StripInterface = BasicStripInterface<LED>, WStripInterface = <LEDW>, etc.
  template<class PixelT>
  class BasicStripInterface
  {
  public:
    BasicStripInterface(uint8_t pin, bool reverse = false)
    : brightness{120}, reverse{reverse}, pin{pin}, timer{0ul} {}
    virtual ~BasicStripInterface() = default;
    uint8_t brightness;
    bool reverse;
    virtual uint16_t length() const = 0;
    virtual void clear(void) = 0;
    void begin(void) { pix::begin(pin); }
    virtual void show(void) = 0;
    virtual void show(uint8_t bright) = 0;
    virtual void fill(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void fill(uint32_t color) = 0;
    virtual void fill(const PixelT& color) = 0;
    virtual void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to) = 0;
    virtual void fillFromTo(uint32_t color, uint16_t from, uint16_t to) = 0;
    virtual void fillFromTo(const PixelT& color, uint16_t from, uint16_t to) = 0;
    virtual PixelT& led(uint16_t i) = 0;
    virtual const PixelT& led(uint16_t i) const = 0;
    inline void setPin(uint8_t p) { pin = p; }
    inline uint8_t getPin(void) const { return pin; }
    // See _extern_channel_ok above for the exact per-platform meaning.
    inline bool ok(void) const { return pix::_extern_channel_ok(pin); }

  protected:
    uint8_t pin;
    uint32_t timer;
  };

  // Statically-sized strip (no heap). PixelT selects the color format (LED/LEDW);
  // ASYNC (ESP32/RP2040 only) enables the send-buffer / non-blocking show().
  template<uint16_t len, class PixelT, bool ASYNC = false>
  class BasicStrip final : public BasicStripInterface<PixelT>
  {
    typedef BasicStripInterface<PixelT> Base;
  public:
    BasicStrip(uint8_t pin = 255, bool reverse = false) : Base{pin, reverse} {}
    uint16_t length() const override final { return len; }
    ~BasicStrip() override {}

    void clear(void) override { memset((void*)leds, 0, len * sizeof(PixelT)); }

#if PIX_STRIP_DOUBLE_BUFFER
    void show(void) override { showImpl(this->brightness, AsyncTag<ASYNC>{}); }
    void show(uint8_t bright) override { showImpl(bright, AsyncTag<ASYNC>{}); }
    // Block until this strip's in-flight (async) transmission finishes.
    void wait(void) { waitImpl(AsyncTag<ASYNC>{}); }
    // True while an async transmission is still on the wire.
    bool busy(void) { return busyImpl(AsyncTag<ASYNC>{}); }
#else
    void show(void) override
    { pix::_extern_timer_show((const uint8_t*)leds, len * sizeof(PixelT), this->pin, this->timer, this->brightness); }
    void show(uint8_t bright) override
    { pix::_extern_timer_show((const uint8_t*)leds, len * sizeof(PixelT), this->pin, this->timer, bright); }
#endif

    void fill(uint8_t r, uint8_t g, uint8_t b) override
    { for (uint16_t i = 0; i < len; ++i) leds[i] = PixelT(r, g, b); }
    void fill(uint32_t color) override
    { for (uint16_t i = 0; i < len; ++i) leds[i] = color; }
    void fill(const PixelT& color) override
    { for (uint16_t i = 0; i < len; ++i) leds[i] = color; }

    // 4-channel fill (RGBW). Only compiles when PixelT has a 4-argument
    // constructor (LEDW); calling it on an RGB strip is a compile error.
    void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
    { for (uint16_t i = 0; i < len; ++i) leds[i] = PixelT(r, g, b, w); }

    void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to) override
    { fillRange(PixelT(r, g, b), from, to); }
    void fillFromTo(uint32_t color, uint16_t from, uint16_t to) override
    { fillRange(PixelT(color), from, to); }
    void fillFromTo(const PixelT& color, uint16_t from, uint16_t to) override
    { fillRange(color, from, to); }

    PixelT& led(uint16_t i) override { return at(i); }
    const PixelT& led(uint16_t i) const override { return at(i); }
    PixelT& operator[](uint16_t i) { return at(i); }
    const PixelT& operator[](uint16_t i) const { return at(i); }

  private:
    PixelT& at(uint16_t i)
    {
      static PixelT voidPixel;
      if (i < len) return this->reverse ? leds[len - 1 - i] : leds[i];
      return voidPixel;
    }
    const PixelT& at(uint16_t i) const
    {
      static PixelT voidPixel;
      if (i < len) return this->reverse ? leds[len - 1 - i] : leds[i];
      return voidPixel;
    }
    void fillRange(const PixelT& color, uint16_t from, uint16_t to)
    {
      if (from > to || to >= len) return;
      if (!this->reverse) { for (uint16_t i = from; i <= to; ++i) leds[i] = color; }
      else { for (uint16_t i = from; i <= to; ++i) leds[len - 1 - i] = color; }
    }
#if PIX_STRIP_DOUBLE_BUFFER
    SendBuf<len, PixelT, ASYNC> _send; // present only when ASYNC == true

    // ASYNC: finish any previous frame, snapshot+scale into _send, fire and return.
    void showImpl(uint8_t bright, AsyncTag<true>)
    {
      pix::_extern_show_async_wait(this->pin, this->timer);
      pix::scaleTo(_send.data, (const uint8_t*)leds, len * sizeof(PixelT), bright);
      pix::_extern_show_async_begin(_send.data, len * sizeof(PixelT), this->pin, this->timer);
    }
    // Blocking: scale straight from the live buffer (no send buffer).
    void showImpl(uint8_t bright, AsyncTag<false>)
    { pix::_extern_timer_show((const uint8_t*)leds, len * sizeof(PixelT), this->pin, this->timer, bright); }
    void waitImpl(AsyncTag<true>) { pix::_extern_show_async_wait(this->pin, this->timer); }
    void waitImpl(AsyncTag<false>) {}
    bool busyImpl(AsyncTag<true>) { return pix::_extern_show_busy(this->pin); }
    bool busyImpl(AsyncTag<false>) { return false; }
#endif
    PixelT leds[len];
  };

  // Heap-backed strip (resizable). Blocking only; declared here, defined in
  // Strip.cpp with explicit instantiations for each pixel format.
  template<class PixelT>
  class BasicStripAllocate final : public BasicStripInterface<PixelT>
  {
  public:
    BasicStripAllocate();
    BasicStripAllocate(uint16_t pixelCount, uint8_t pin = 255, bool reverse = false);
    ~BasicStripAllocate() override;
    uint16_t length() const override;
    void clear(void) override;
    void show(void) override;
    void show(uint8_t bright) override;
    void fill(uint8_t r, uint8_t g, uint8_t b) override;
    void fill(uint32_t color) override;
    void fill(const PixelT& color) override;
    void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to) override;
    void fillFromTo(uint32_t color, uint16_t from, uint16_t to) override;
    void fillFromTo(const PixelT& color, uint16_t from, uint16_t to) override;
    PixelT& led(uint16_t i) override;
    const PixelT& led(uint16_t i) const override;
    PixelT& operator[](uint16_t i);
    const PixelT& operator[](uint16_t i) const;
    void changeLength(uint16_t l);

  private:
    PixelT* leds;
    uint16_t len;
  };

  // ---- RGB (LED) facade ----------------------------------------------------
  using StripInterface = BasicStripInterface<LED>;
  using StripHandler = StripInterface*;
  using StripAllocate = BasicStripAllocate<LED>;

  // ---- RGBW (LEDW) facade --------------------------------------------------
  using WStripInterface = BasicStripInterface<LEDW>;
  using WStripHandler = WStripInterface*;
  using WStripAllocate = BasicStripAllocate<LEDW>;

#if PIX_STRIP_DOUBLE_BUFFER
  // ESP32/RP2040: async is opt-out per strip (default on). Strip<N> is
  // double-buffered/non-blocking; Strip<N, false> is single-buffered/blocking.
  template<uint16_t S, bool A = true>
  using Strip = BasicStrip<S, LED, A>;
  template<uint16_t S, bool A = true>
  using WStrip = BasicStrip<S, LEDW, A>;
#else
  // AVR/ESP8266: no async - the ASYNC parameter is intentionally not exposed.
  template<uint16_t S>
  using Strip = BasicStrip<S, LED>;
  template<uint16_t S>
  using WStrip = BasicStrip<S, LEDW>;
#endif
}


#endif // _STRIP_H

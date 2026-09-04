#ifdef ARDUINO_ARCH_RP2040

#include "Strip.h"
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/clocks.h>
#include <pico/mutex.h>

namespace pix
{
  namespace
  {
    // WS2812B via the pico-examples ws2812 PIO program shape, retimed to
    // 25 cycles per bit @ 20 MHz (50 ns/cycle): T0H 400ns / T0L 850ns /
    // T1H 800ns / T1L 450ns - every edge sits >=150 ns away from the limits
    // in protocol.txt instead of on them, so clock jitter (e.g. the PIO
    // fractional divider) cannot push the signal out of spec. Bit period
    // stays 1250 ns (800 kHz). The PIO state machine shapes the waveform; each
    // FIFO word's top byte is shifted out MSB-first (autopull threshold 8).
    constexpr float PIO_CLOCK_HZ = 20000000.0f;

    const uint16_t ws2812Instructions[] = {
      0x6821, // 0: out x, 1     side 0 [8]   ; 9 cycles low tail of previous bit (T1L=450ns)
      0x1723, // 1: jmp !x, 3    side 1 [7]   ; 8 cycles high (T0H=400ns)
      0x1700, // 2: jmp 0        side 1 [7]   ; +8 cycles high for a 1 (T1H=800ns)
      0xa742, // 3: nop          side 0 [7]   ; +8 cycles low for a 0 (T0L=850ns)
    };
    const pio_program_t ws2812Program = { ws2812Instructions, 4, -1 };

    struct ChannelSlot
    {
      uint8_t pin = 255;
      bool inUse = false;
      PIO pio = nullptr;
      int sm = -1;
      int dma = -1; // used only by the async (DMA) path
      uint8_t pioIndex = 0; // which PIO block (index into g_programOffset)
      // micros() at last claim/transmit start; used to pick which idle slot
      // to steal first (oldest activity goes first).
      uint32_t lastUsed = 0;
    };

    constexpr uint8_t MAX_SLOTS = NUM_PIOS * 4;
    ChannelSlot g_slots[MAX_SLOTS];
    int8_t g_programOffset[NUM_PIOS];
    mutex_t g_registryMutex;
    // Global ctor runs before setup(), while still single-threaded/single-core,
    // so the mutex and offsets are ready before any task can race on them.
    struct GlobalsInit
    {
      GlobalsInit()
      {
        mutex_init(&g_registryMutex);
        for (uint8_t i = 0; i < NUM_PIOS; ++i) g_programOffset[i] = -1;
      }
    } g_globalsInit;
    uint32_t g_freeShowTimer = 0;

    inline uint8_t fastScale(uint8_t c, uint8_t bright)
    {
      return (uint8_t)(((uint16_t)c * bright + c) >> 8);
    }

    // Non-claiming lookup: does `pin` currently own a slot? Used by wait()/
    // busy(), which must never allocate/steal a channel just to answer a
    // status query - only a real show() (acquireChannel) may do that.
    ChannelSlot* findOwnedChannel(uint8_t pin)
    {
      if (pin == 255) return nullptr;
      mutex_enter_blocking(&g_registryMutex);
      ChannelSlot* found = nullptr;
      for (uint8_t i = 0; i < MAX_SLOTS; ++i)
      {
        if (g_slots[i].inUse && g_slots[i].pin == pin) { found = &g_slots[i]; break; }
      }
      mutex_exit(&g_registryMutex);
      return found;
    }

    inline uint32_t stallMask(const ChannelSlot& s)
    {
      return 1u << (PIO_FDEBUG_TXSTALL_LSB + s.sm);
    }

    // The TXSTALL flag is sticky, so it must be cleared at the START of a frame;
    // it then re-asserts only once the state machine stalls on an empty FIFO -
    // i.e. the last bit has actually left the wire (FIFO "fed" != frame done).
    inline void clearTxStall(ChannelSlot& s) { s.pio->fdebug = stallMask(s); }
    inline void waitTxStallSet(ChannelSlot& s) { while (!(s.pio->fdebug & stallMask(s))) {} }

    // Non-blocking: true once the DMA has finished and the state machine has
    // drained its FIFO (frame fully on the wire) - i.e. safe to steal this slot.
    inline bool channelIdle(ChannelSlot& s)
    {
      return !dma_channel_is_busy((uint)s.dma) && (s.pio->fdebug & stallMask(s));
    }

    // Blocking path (ASYNC=false + free pix::show): the CPU scales each byte on
    // the fly and pushes it into the PIO FIFO. No buffer, no DMA, zero heap. The
    // PIO shapes the timing, so this is not cycle-critical and needs no interrupt
    // lock; the 8-deep TX FIFO (~80 us) absorbs any scheduling jitter.
    void feedFifoBlocking(ChannelSlot* slot, const uint8_t* bytes, uint16_t byteLen, uint8_t effBright)
    {
      if (slot == nullptr || bytes == nullptr || byteLen == 0) return;
      slot->lastUsed = micros();
      PIO pio = slot->pio;
      const uint sm = (uint)slot->sm;
      clearTxStall(*slot);
      for (uint32_t i = 0; i < byteLen; ++i)
      {
        uint8_t c = fastScale(bytes[i], effBright); // effBright == 255 -> identity
        pio_sm_put_blocking(pio, sm, ((uint32_t)c) << 24); // top byte, MSB-first
      }
      waitTxStallSet(*slot);
    }

    // Async path (ASYNC=true): a single DMA streams the pre-scaled bytes into the
    // FIFO and returns immediately; completion is polled later in wait/busy.
    void startDma(ChannelSlot* slot, const uint8_t* bytes, uint16_t byteLen)
    {
      slot->lastUsed = micros();
      clearTxStall(*slot);
      dma_channel_config c = dma_channel_get_default_config((uint)slot->dma);
      channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
      channel_config_set_read_increment(&c, true);
      channel_config_set_write_increment(&c, false);
      channel_config_set_dreq(&c, pio_get_dreq(slot->pio, (uint)slot->sm, true));
      dma_channel_configure((uint)slot->dma, &c, &slot->pio->txf[slot->sm], bytes, byteLen, true);
    }

    PIO pioInstance(uint8_t i)
    {
      switch (i)
      {
        case 0: return pio0;
        case 1: return pio1;
        #if NUM_PIOS > 2
        case 2: return pio2;
        #endif
        default: return nullptr;
      }
    }

    // Shared by initChannel (first claim) and rebindChannel (steal): sets up
    // the state machine's program/timing/pins. Caller enables the SM after.
    void configureStateMachine(PIO pio, uint sm, uint offset, uint8_t pin)
    {
      pio_sm_config c = pio_get_default_sm_config();
      sm_config_set_wrap(&c, offset, offset + 3);
      sm_config_set_sideset(&c, 1, false, false);
      sm_config_set_sideset_pins(&c, pin);
      sm_config_set_out_shift(&c, false, true, 8); // shift left (MSB first), autopull every byte
      sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
      sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / PIO_CLOCK_HZ);
      pio_gpio_init(pio, pin);
      pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
      pio_sm_init(pio, sm, offset, &c);
    }

    bool initChannel(ChannelSlot& slot, uint8_t pin)
    {
      for (uint8_t p = 0; p < NUM_PIOS; ++p)
      {
        PIO pio = pioInstance(p);
        if (pio == nullptr) continue;
        if (g_programOffset[p] < 0)
        {
          if (!pio_can_add_program(pio, &ws2812Program)) continue;
          g_programOffset[p] = (int8_t)pio_add_program(pio, &ws2812Program);
        }
        int sm = pio_claim_unused_sm(pio, false);
        if (sm < 0) continue;
        int dma = dma_claim_unused_channel(false);
        if (dma < 0)
        {
          pio_sm_unclaim(pio, (uint)sm);
          return false; // DMA channels are chip-global, another PIO won't help
        }
        configureStateMachine(pio, (uint)sm, (uint)g_programOffset[p], pin);
        pio_sm_set_enabled(pio, (uint)sm, true);
        slot.pio = pio;
        slot.sm = sm;
        slot.dma = dma;
        slot.pioIndex = p;
        return true;
      }
      return false;
    }

    // Cheap channel reassignment: same PIO block/SM/DMA channel, only the
    // side-set GPIO changes. No program re-add, no DMA re-claim - just
    // reconfigure and restart the state machine on the new pin. Only called
    // on a slot already confirmed idle (see tryAcquireChannel), so this never
    // disturbs an in-flight frame.
    void rebindChannel(ChannelSlot& slot, uint8_t newPin)
    {
      pio_sm_set_enabled(slot.pio, (uint)slot.sm, false);
      configureStateMachine(slot.pio, (uint)slot.sm, (uint)g_programOffset[slot.pioIndex], newPin);
      pio_sm_set_enabled(slot.pio, (uint)slot.sm, true);
      slot.pin = newPin;
    }

    // Single, non-blocking attempt: fast path (pin already owns a slot), else
    // a never-claimed slot, else steal the least-recently-used *idle* slot
    // (never one mid-transmission) and rebind it to `pin`. Returns nullptr
    // only when every channel is currently busy.
    ChannelSlot* tryAcquireChannel(uint8_t pin)
    {
      if (pin == 255) return nullptr;
      mutex_enter_blocking(&g_registryMutex);
      ChannelSlot* found = nullptr;
      ChannelSlot* freeSlot = nullptr;
      for (uint8_t i = 0; i < MAX_SLOTS; ++i)
      {
        ChannelSlot& slot = g_slots[i];
        if (slot.inUse && slot.pin == pin) { found = &slot; break; }
        if (!slot.inUse && !freeSlot) freeSlot = &slot;
      }
      if (!found && freeSlot && initChannel(*freeSlot, pin))
      {
        freeSlot->pin = pin;
        freeSlot->inUse = true;
        found = freeSlot;
      }
      if (!found && !freeSlot) // pool full: steal the oldest idle channel
      {
        ChannelSlot* lru = nullptr;
        for (uint8_t i = 0; i < MAX_SLOTS; ++i)
        {
          ChannelSlot& slot = g_slots[i];
          if (!channelIdle(slot)) continue;
          if (!lru || (int32_t)(slot.lastUsed - lru->lastUsed) < 0) lru = &slot;
        }
        if (lru) { rebindChannel(*lru, pin); lru->lastUsed = micros(); found = lru; }
      }
      mutex_exit(&g_registryMutex);
      return found;
    }

    // Blocking wrapper - the queue: if every channel is busy right now, wait
    // (yielding to other tasks/core) until one goes idle, then rebind it
    // here. Only reached with more concurrently-live pins than physical PIO
    // channels; see the "Channel pooling" section in README.
    ChannelSlot* acquireChannel(uint8_t pin)
    {
      ChannelSlot* slot;
      while ((slot = tryAcquireChannel(pin)) == nullptr) yield();
      return slot;
    }
  } // namespace

  void _begin_periferials(uint8_t pin)
  {
    tryAcquireChannel(pin); // non-blocking: begin() must never stall setup()
  }

  bool _extern_channel_ok(uint8_t pin)
  {
    return tryAcquireChannel(pin) != nullptr;
  }

  // Byte-oriented seam: byteLen = pixels * bytes-per-pixel (3 for RGB, 4 for RGBW).
  void _extern_timer_show(const uint8_t* bytes, uint16_t byteLen, uint8_t pin, uint32_t& timer, uint8_t bright)
  {
    ChannelSlot* slot = acquireChannel(pin); // may queue: see "Channel pooling" in README
    while (micros() - timer < 50) {}
    feedFifoBlocking(slot, bytes, byteLen, bright);
    timer = micros();
  }

  void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright, bool scale)
  {
    ChannelSlot* slot = acquireChannel(pin);
    while (micros() - g_freeShowTimer < 50) {}
    feedFifoBlocking(slot, reinterpret_cast<const uint8_t*>(leds), (uint16_t)(len * 3), scale ? bright : 255);
    g_freeShowTimer = micros();
  }

  // --- Non-blocking (async) seam: bytes are already scaled by the strip ---

  void _extern_show_async_begin(const uint8_t* bytes, uint16_t byteLen, uint8_t pin, uint32_t& timer)
  {
    ChannelSlot* slot = acquireChannel(pin); // may queue: see "Channel pooling" in README
    if (slot == nullptr || bytes == nullptr || byteLen == 0) return;
    while (micros() - timer < 50) {} // WS2812B latch gap since the previous frame
    startDma(slot, bytes, byteLen);
  }

  void _extern_show_async_wait(uint8_t pin, uint32_t& timer)
  {
    // Non-claiming: if `pin` doesn't currently own a slot, its last frame (if
    // any) already finished (an in-flight channel can never be stolen), so
    // there is nothing to wait for.
    ChannelSlot* slot = findOwnedChannel(pin);
    if (slot == nullptr) return;
    while (dma_channel_is_busy((uint)slot->dma)) { yield(); }
    waitTxStallSet(*slot); // TXSTALL was cleared at startDma; set == frame done
    timer = micros();
  }

  bool _extern_show_busy(uint8_t pin)
  {
    ChannelSlot* slot = findOwnedChannel(pin);
    if (slot == nullptr) return false;
    return !channelIdle(*slot);
  }
}

#endif // ARDUINO_ARCH_RP2040

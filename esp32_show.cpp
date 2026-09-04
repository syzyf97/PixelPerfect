#if defined(ESP_PLATFORM) || defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

#include "Strip.h"
#include <esp_idf_version.h>
#include <esp_err.h>
#include <esp_log.h>
#include <soc/soc_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#if ESP_IDF_VERSION_MAJOR >= 5
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#else
#include <driver/rmt.h>
#endif

#ifndef SOC_RMT_CHANNELS_PER_GROUP
#define SOC_RMT_CHANNELS_PER_GROUP 8
#endif

namespace pix
{
  namespace
  {
    constexpr const char* TAG = "PixelPerfect";

    // WS2812B bit timings @ 10MHz RMT resolution (100ns/tick), see protocol.txt
    constexpr uint32_t RMT_RESOLUTION_HZ = 10000000UL;
    constexpr uint16_t T0H_TICKS = 3;   // 300 ns
    constexpr uint16_t T0L_TICKS = 9;   // 900 ns
    constexpr uint16_t T1H_TICKS = 9;   // 900 ns
    constexpr uint16_t T1L_TICKS = 3;   // 300 ns
    constexpr uint16_t RESET_TICKS = 500; // >=50 us low, mirrors the AVR latch gap

    #if ESP_IDF_VERSION_MAJOR >= 5
      #ifdef SOC_RMT_MEM_WORDS_PER_CHANNEL
        constexpr size_t RMT_MEM_BLOCK_SYMBOLS = SOC_RMT_MEM_WORDS_PER_CHANNEL;
      #else
        constexpr size_t RMT_MEM_BLOCK_SYMBOLS = 64;
      #endif
    #endif

    struct ChannelSlot
    {
      uint8_t pin = 255;
      bool inUse = false;
      // micros() at last claim/transmit start; used to pick which idle slot
      // to steal first (oldest activity goes first). Only meaningful where
      // pooling/stealing happens (IDF4, RP2040); harmless dead weight on IDF5.
      uint32_t lastUsed = 0;
      #if ESP_IDF_VERSION_MAJOR >= 5
        rmt_channel_handle_t channel = nullptr;
        rmt_encoder_handle_t encoder = nullptr;
      #else
        rmt_channel_t channel = RMT_CHANNEL_0;
        uint8_t bright = 255; // read by the translator via the channel context
      #endif
    };

    ChannelSlot g_slots[SOC_RMT_CHANNELS_PER_GROUP];
    SemaphoreHandle_t g_registryMutex = xSemaphoreCreateMutex();
    uint32_t g_freeShowTimer = 0;

    inline uint8_t fastScale(uint8_t c, uint8_t bright)
    {
      return (uint8_t)(((uint16_t)c * bright + c) >> 8); // matches atmega_show.cpp
    }

    // Non-claiming lookup: does `pin` currently own a slot? Used by wait()/
    // busy(), which must never allocate/steal a channel just to answer a
    // status query - only a real show() (acquireChannel) may do that.
    ChannelSlot* findOwnedChannel(uint8_t pin)
    {
      if (pin == 255) return nullptr;
      xSemaphoreTake(g_registryMutex, portMAX_DELAY);
      ChannelSlot* found = nullptr;
      for (uint8_t i = 0; i < SOC_RMT_CHANNELS_PER_GROUP; ++i)
      {
        if (g_slots[i].inUse && g_slots[i].pin == pin) { found = &g_slots[i]; break; }
      }
      xSemaphoreGive(g_registryMutex);
      return found;
    }

    #if ESP_IDF_VERSION_MAJOR >= 5

    // Custom RMT encoder that scales brightness on the fly, straight from the raw
    // GRB buffer - no pre-scaled copy of the strip is kept. Each byte is scaled and
    // expanded into its 8 line symbols in a fixed 8-symbol buffer (O(1) memory,
    // independent of strip length), streamed out with a copy encoder; a second copy
    // encoder emits the trailing reset gap. Runs in the RMT ISR.
    struct LedStripEncoder
    {
      rmt_encoder_t base;
      rmt_encoder_t* dataCopy;   // streams the current byte's 8 symbols
      rmt_encoder_t* resetCopy;  // streams the trailing reset symbol
      rmt_symbol_word_t bit0, bit1, resetCode;
      rmt_symbol_word_t sym8[8]; // symbols of the byte currently being emitted
      size_t bytePos;
      bool byteBuilt;
      int state;
      uint8_t bright;
    };

    size_t ledStripEncode(rmt_encoder_t* encoder, rmt_channel_handle_t channel, const void* data, size_t dataSize, rmt_encode_state_t* retState)
    {
      LedStripEncoder* self = reinterpret_cast<LedStripEncoder*>(encoder);
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
      rmt_encode_state_t st = RMT_ENCODING_RESET;
      int overall = 0;
      size_t encoded = 0;

      if (self->state == 0) // scaled pixel data
      {
        while (self->bytePos < dataSize)
        {
          if (!self->byteBuilt)
          {
            uint8_t c = fastScale(bytes[self->bytePos], self->bright);
            for (int b = 0; b < 8; ++b)
              self->sym8[b] = (c & (0x80 >> b)) ? self->bit1 : self->bit0;
            self->byteBuilt = true;
          }
          encoded += self->dataCopy->encode(self->dataCopy, channel, self->sym8, sizeof(self->sym8), &st);
          if (st & RMT_ENCODING_COMPLETE) { ++self->bytePos; self->byteBuilt = false; }
          if (st & RMT_ENCODING_MEM_FULL) { *retState = RMT_ENCODING_MEM_FULL; return encoded; }
        }
        self->state = 1;
      }
      if (self->state == 1) // trailing reset gap
      {
        encoded += self->resetCopy->encode(self->resetCopy, channel, &self->resetCode, sizeof(self->resetCode), &st);
        if (st & RMT_ENCODING_COMPLETE) { self->state = 0; overall |= RMT_ENCODING_COMPLETE; }
        if (st & RMT_ENCODING_MEM_FULL) overall |= RMT_ENCODING_MEM_FULL;
      }
      *retState = (rmt_encode_state_t)overall;
      return encoded;
    }

    esp_err_t ledStripEncoderReset(rmt_encoder_t* encoder)
    {
      LedStripEncoder* self = reinterpret_cast<LedStripEncoder*>(encoder);
      rmt_encoder_reset(self->dataCopy);
      rmt_encoder_reset(self->resetCopy);
      self->state = 0;
      self->bytePos = 0;
      self->byteBuilt = false;
      return ESP_OK;
    }

    esp_err_t ledStripEncoderDel(rmt_encoder_t* encoder)
    {
      LedStripEncoder* self = reinterpret_cast<LedStripEncoder*>(encoder);
      rmt_del_encoder(self->dataCopy);
      rmt_del_encoder(self->resetCopy);
      return ESP_OK; // the LedStripEncoder itself is statically owned (g_encoders)
    }

    // One statically-allocated encoder per channel slot - our code never touches
    // the heap. (The RMT SDK still allocates the copy encoders / channel internally.)
    LedStripEncoder g_encoders[SOC_RMT_CHANNELS_PER_GROUP];

    bool initLedStripEncoder(LedStripEncoder& self)
    {
      self.base.encode = ledStripEncode;
      self.base.reset = ledStripEncoderReset;
      self.base.del = ledStripEncoderDel;
      self.state = 0;
      self.bytePos = 0;
      self.byteBuilt = false;
      self.bright = 255;
      self.bit0.level0 = 1; self.bit0.duration0 = T0H_TICKS;
      self.bit0.level1 = 0; self.bit0.duration1 = T0L_TICKS;
      self.bit1.level0 = 1; self.bit1.duration0 = T1H_TICKS;
      self.bit1.level1 = 0; self.bit1.duration1 = T1L_TICKS;
      self.resetCode.level0 = 0; self.resetCode.duration0 = RESET_TICKS;
      self.resetCode.level1 = 0; self.resetCode.duration1 = 0;

      rmt_copy_encoder_config_t copyCfg = {};
      if (rmt_new_copy_encoder(&copyCfg, &self.dataCopy) != ESP_OK) return false;
      if (rmt_new_copy_encoder(&copyCfg, &self.resetCopy) != ESP_OK)
      {
        rmt_del_encoder(self.dataCopy);
        return false;
      }
      return true;
    }

    bool initChannel(ChannelSlot& slot, uint8_t slotIndex, uint8_t pin)
    {
      rmt_tx_channel_config_t chCfg = {};
      chCfg.gpio_num = (gpio_num_t)pin;
      chCfg.clk_src = RMT_CLK_SRC_DEFAULT;
      chCfg.resolution_hz = RMT_RESOLUTION_HZ;
      chCfg.mem_block_symbols = RMT_MEM_BLOCK_SYMBOLS;
      chCfg.trans_queue_depth = 4;
      if (rmt_new_tx_channel(&chCfg, &slot.channel) != ESP_OK) return false;
      if (rmt_enable(slot.channel) != ESP_OK) return false;
      if (!initLedStripEncoder(g_encoders[slotIndex])) return false;
      slot.encoder = &g_encoders[slotIndex].base;
      return true;
    }

    #else // ESP_IDF_VERSION_MAJOR < 5 (legacy driver/rmt.h)

    // Converts raw GRB bytes into RMT items on demand, scaling brightness per byte,
    // so neither a pre-scaled copy nor a full item array is ever allocated (O(1)).
    // The RMT driver calls this to refill its buffer (in part from ISR context), so
    // it is placed in IRAM. Frame latch relies on idle_level = LOW plus the leading
    // >=50us gap in _extern_timer_show, so no explicit reset item is emitted here.
    void IRAM_ATTR ws2812Translator(const void* src, rmt_item32_t* dest, size_t src_size,
                                    size_t wanted_num, size_t* translated_size, size_t* item_num)
    {
      if (src == nullptr || dest == nullptr || wanted_num < 8)
      {
        *translated_size = 0;
        *item_num = 0;
        return;
      }
      void* ctx = nullptr;
      rmt_translator_get_context(item_num, &ctx);
      const uint8_t bright = ctx ? reinterpret_cast<ChannelSlot*>(ctx)->bright : 255;

      rmt_item32_t bit0, bit1;
      bit0.level0 = 1; bit0.duration0 = T0H_TICKS; bit0.level1 = 0; bit0.duration1 = T0L_TICKS;
      bit1.level0 = 1; bit1.duration0 = T1H_TICKS; bit1.level1 = 0; bit1.duration1 = T1L_TICKS;

      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(src);
      size_t num = 0, sz = 0;
      while (sz < src_size && num + 8 <= wanted_num)
      {
        uint8_t c = fastScale(bytes[sz], bright);
        for (int b = 0; b < 8; ++b)
          dest[num + b] = (c & (0x80 >> b)) ? bit1 : bit0;
        num += 8;
        ++sz;
      }
      *translated_size = sz;
      *item_num = num;
    }

    bool initChannel(ChannelSlot& slot, uint8_t slotIndex, uint8_t pin)
    {
      rmt_channel_t ch = (rmt_channel_t)slotIndex;
      rmt_config_t config = {};
      config.rmt_mode = RMT_MODE_TX;
      config.channel = ch;
      config.gpio_num = (gpio_num_t)pin;
      config.mem_block_num = 1;
      config.clk_div = 8; // 80 MHz APB / 8 = 10 MHz -> 100 ns/tick
      config.tx_config.loop_en = false;
      config.tx_config.carrier_en = false;
      config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
      config.tx_config.idle_output_en = true;
      if (rmt_config(&config) != ESP_OK) return false;
      if (rmt_driver_install(config.channel, 0, 0) != ESP_OK) return false;
      if (rmt_translator_init(config.channel, ws2812Translator) != ESP_OK) return false;
      rmt_translator_set_context(config.channel, &slot);
      slot.channel = ch;
      return true;
    }

    #endif // ESP_IDF_VERSION_MAJOR

    // Start transmitting byteCount bytes from `bytes`, scaling each by effBright
    // (255 = verbatim). Non-blocking: returns once the transfer is queued; the
    // RMT hardware/ISR streams the data, so `bytes` must stay valid until done.
    void startTransmit(ChannelSlot* slot, const uint8_t* bytes, uint16_t byteCount, uint8_t effBright)
    {
      #if ESP_IDF_VERSION_MAJOR >= 5
        LedStripEncoder* enc = reinterpret_cast<LedStripEncoder*>(slot->encoder);
        enc->bright = effBright;
        enc->bytePos = 0;
        enc->byteBuilt = false;
        enc->state = 0;
        rmt_transmit_config_t txConf = {};
        rmt_transmit(slot->channel, slot->encoder, bytes, byteCount, &txConf);
      #else
        slot->bright = effBright;
        rmt_write_sample(slot->channel, bytes, byteCount, false); // false = don't block
      #endif
      slot->lastUsed = micros();
    }

    void waitTransmit(ChannelSlot* slot)
    {
      #if ESP_IDF_VERSION_MAJOR >= 5
        rmt_tx_wait_all_done(slot->channel, -1);
      #else
        rmt_wait_tx_done(slot->channel, portMAX_DELAY);
      #endif
    }

    bool busyTransmit(ChannelSlot* slot)
    {
      #if ESP_IDF_VERSION_MAJOR >= 5
        return rmt_tx_wait_all_done(slot->channel, 0) == ESP_ERR_TIMEOUT;
      #else
        return rmt_wait_tx_done(slot->channel, 0) == ESP_ERR_TIMEOUT;
      #endif
    }

    // Blocking scale-and-show straight from the live byte buffer (no snapshot).
    void transmitBlocking(ChannelSlot* slot, const uint8_t* bytes, uint16_t byteLen, uint8_t effBright)
    {
      if (slot == nullptr || bytes == nullptr || byteLen == 0) return;
      startTransmit(slot, bytes, byteLen, effBright);
      waitTransmit(slot);
    }

    #if ESP_IDF_VERSION_MAJOR >= 5

    // No public API exists on this driver to cheaply rebind an existing
    // rmt_tx channel to a different GPIO (the only way is rmt_del_channel +
    // rmt_new_tx_channel + recreating the encoder - too expensive to do
    // transparently on every frame), so channels are NOT pooled here: once
    // all SOC_RMT_CHANNELS_PER_GROUP channels belong to other pins, further
    // pins simply cannot get one. Logs once per pin instead of failing
    // silently; strip.ok() surfaces the same condition without serial output.
    uint64_t g_warnedPins = 0;

    // Serializes lookup/creation of registry entries; a real WS2812 transmission
    // is never started from inside this lock (see transmitBlocking), so this never
    // blocks other tasks for longer than a plain array scan + (rare) driver install.
    ChannelSlot* acquireChannel(uint8_t pin)
    {
      if (pin == 255) return nullptr;
      xSemaphoreTake(g_registryMutex, portMAX_DELAY);
      ChannelSlot* found = nullptr;
      ChannelSlot* freeSlot = nullptr;
      uint8_t freeIndex = 0;
      for (uint8_t i = 0; i < SOC_RMT_CHANNELS_PER_GROUP; ++i)
      {
        ChannelSlot& slot = g_slots[i];
        if (slot.inUse && slot.pin == pin) { found = &slot; break; }
        if (!slot.inUse && !freeSlot) { freeSlot = &slot; freeIndex = i; }
      }
      if (!found && freeSlot && initChannel(*freeSlot, freeIndex, pin))
      {
        freeSlot->pin = pin;
        freeSlot->inUse = true;
        found = freeSlot;
      }
      xSemaphoreGive(g_registryMutex);
      if (!found && pin < 64 && !(g_warnedPins & (1ULL << pin)))
      {
        g_warnedPins |= (1ULL << pin);
        ESP_LOGE(TAG, "RMT channel pool exhausted (%d channels): pin %d got none. This "
                      "ESP-IDF's RMT driver (IDF5+) cannot share channels between pins - "
                      "see the \"Channel pooling\" section in README.",
                      (int)SOC_RMT_CHANNELS_PER_GROUP, (int)pin);
      }
      return found;
    }

    // No pooling on this driver: a single attempt IS the whole story.
    inline ChannelSlot* tryAcquireChannel(uint8_t pin) { return acquireChannel(pin); }

    #else // ESP_IDF_VERSION_MAJOR < 5

    // Single, non-blocking attempt: fast path (pin already owns a slot), else
    // a never-claimed slot, else steal the least-recently-used *idle* slot
    // (never one still mid-transmission) and rebind it to `pin` via the
    // legacy driver's cheap rmt_set_gpio (no channel/encoder teardown).
    // Returns nullptr only when every channel is currently transmitting.
    ChannelSlot* tryAcquireChannel(uint8_t pin)
    {
      if (pin == 255) return nullptr;
      xSemaphoreTake(g_registryMutex, portMAX_DELAY);
      ChannelSlot* found = nullptr;
      ChannelSlot* freeSlot = nullptr;
      uint8_t freeIndex = 0;
      for (uint8_t i = 0; i < SOC_RMT_CHANNELS_PER_GROUP; ++i)
      {
        ChannelSlot& slot = g_slots[i];
        if (slot.inUse && slot.pin == pin) { found = &slot; break; }
        if (!slot.inUse && !freeSlot) { freeSlot = &slot; freeIndex = i; }
      }
      if (!found && freeSlot && initChannel(*freeSlot, freeIndex, pin))
      {
        freeSlot->pin = pin;
        freeSlot->inUse = true;
        found = freeSlot;
      }
      if (!found && !freeSlot) // pool full: steal the oldest idle channel
      {
        ChannelSlot* lru = nullptr;
        for (uint8_t i = 0; i < SOC_RMT_CHANNELS_PER_GROUP; ++i)
        {
          ChannelSlot& slot = g_slots[i];
          if (busyTransmit(&slot)) continue;
          if (!lru || (int32_t)(slot.lastUsed - lru->lastUsed) < 0) lru = &slot;
        }
        if (lru && rmt_set_gpio(lru->channel, RMT_MODE_TX, (gpio_num_t)pin, false) == ESP_OK)
        {
          lru->pin = pin;
          lru->lastUsed = micros();
          found = lru;
        }
      }
      xSemaphoreGive(g_registryMutex);
      return found;
    }

    // Blocking wrapper - the queue: if every channel is busy right now, wait
    // (yielding to other tasks) until one goes idle, then rebind it here.
    // Only reached with more concurrently-live pins than physical RMT
    // channels; see the "Channel pooling" section in README.
    ChannelSlot* acquireChannel(uint8_t pin)
    {
      ChannelSlot* slot;
      while ((slot = tryAcquireChannel(pin)) == nullptr) vTaskDelay(1);
      return slot;
    }

    #endif // ESP_IDF_VERSION_MAJOR
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
    transmitBlocking(slot, bytes, byteLen, bright);
    timer = micros();
  }

  void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright, bool scale)
  {
    ChannelSlot* slot = acquireChannel(pin);
    while (micros() - g_freeShowTimer < 50) {}
    transmitBlocking(slot, reinterpret_cast<const uint8_t*>(leds), (uint16_t)(len * 3), scale ? bright : 255);
    g_freeShowTimer = micros();
  }

  // --- Non-blocking (async) seam: bytes are already scaled by the strip ---

  void _extern_show_async_begin(const uint8_t* bytes, uint16_t byteLen, uint8_t pin, uint32_t& timer)
  {
    ChannelSlot* slot = acquireChannel(pin); // may queue: see "Channel pooling" in README
    if (slot == nullptr || bytes == nullptr || byteLen == 0) return;
    while (micros() - timer < 50) {} // WS2812B latch gap since the previous frame
    startTransmit(slot, bytes, byteLen, 255); // 255 -> verbatim (already scaled)
  }

  void _extern_show_async_wait(uint8_t pin, uint32_t& timer)
  {
    // Non-claiming: if `pin` doesn't currently own a slot, its last frame (if
    // any) already finished (an in-flight channel can never be stolen), so
    // there is nothing to wait for.
    ChannelSlot* slot = findOwnedChannel(pin);
    if (slot == nullptr) return;
    waitTransmit(slot);
    timer = micros();
  }

  bool _extern_show_busy(uint8_t pin)
  {
    ChannelSlot* slot = findOwnedChannel(pin);
    if (slot == nullptr) return false;
    return busyTransmit(slot);
  }
}

#endif // ESP32

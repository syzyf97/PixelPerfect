# PixelPerfect

*Read this in your language — expand a section below.*
🇬🇧 English · 🇵🇱 Polski · 🇩🇪 Deutsch · 🇫🇷 Français · 🇮🇹 Italiano · 🇪🇸 Español · 🇷🇺 Русский

<details open>
<summary><b>🇬🇧 English</b></summary>

<br>

A lightweight C++ library for driving **WS2812B** ("NeoPixel") addressable LED strips. It aims to be simpler than FastLED while avoiding dynamic memory allocation on the hot path: the default strip type keeps its pixel buffer in a fixed, statically sized array.

The same source builds unchanged across several targets, each using the best output mechanism the hardware offers:

| Platform | Framework | Output mechanism | Source |
| :------- | :-------- | :--------------- | :----- |
| AVR (e.g. ATmega2560) | Arduino | Cycle-counted bit-banging (inline assembly) | `atmega_show.cpp` |
| ESP32 (all variants) | Arduino **or** ESP-IDF | Hardware RMT peripheral | `esp32_show.cpp` |
| ESP8266 | Arduino | CCOUNT-timed bit-banging | `esp8266_show.cpp` |
| RP2040 / Raspberry Pi Pico | Arduino (arduino-pico) | PIO state machine + DMA | `rp2040_show.cpp` |

The correct backend is selected automatically at compile time from predefined framework macros — you never choose it manually.

## Author

- [@syzyf97](https://www.github.com/syzyf97)

## Features

- Fixed-size, allocation-free strip (`Strip<N>`) plus an optional heap-backed variant (`StripAllocate`).
- **RGB and RGBW** (e.g. SK6812) as two separate, type-safe families: `Strip`/`LED` and `WStrip`/`LEDW`.
- **Non-blocking refresh** on ESP32/RP2040 (`Strip<N>` is async by default), so several strips can transmit at once; `wait()`/`busy()` to synchronize.
- Per-strip and per-call brightness scaling.
- Convenience color helpers: HSV, color temperature (Kelvin), and gamma correction, all backed by lookup tables in flash by default.
- Grouping of several strips behind one logical index space (`StripGroup` / `WStripGroup`).
- FreeRTOS-friendly on ESP32 and RP2040: a refresh never stalls the scheduler or the second core.

## Color formats: RGB and RGBW

Two parallel, type-safe families share one implementation:

| | Pixel | Strip | Group | Heap variant |
| :-- | :-- | :-- | :-- | :-- |
| **RGB** (WS2812B) | `LED` (3 B) | `Strip<N>` | `StripGroup<M>` | `StripAllocate` |
| **RGBW** (SK6812) | `LEDW` (4 B) | `WStrip<N>` | `WStripGroup<M>` | `WStripAllocate` |

The API is identical; RGBW just adds the white channel:

```cpp
WStrip<30> strip{2};
strip.begin();
strip.fill(0, 0, 0, 255);    // pure white through the W channel
strip.fill(255, 0, 0);       // red; the 3-argument fill sets W = 0
strip[10] = {0, 0, 255, 80}; // per-pixel R,G,B,W
strip[11] = hsvW(1000);      // RGBW color helpers -> LEDW (W = 0)
strip.fill(kelvinW(3000));   // no cast, no ambiguity
```

An `LEDW` is a packed 4-byte value; its 32-bit form is `0xRRGGBBWW` and `operator[]` indexes `0=R, 1=G, 2=B, 3=W`. Use the RGBW color helpers `hsvW()` / `kelvinW()` — they return an `LEDW` (with `W = 0`), so RGBW code never touches the `LED` type. To promote an existing `LED` on purpose, call `led.toLEDW()` (there is no implicit RGB→RGBW conversion: `strip[i] = hsv(x)` is a deliberate compile error nudging you to `hsvW` / `toLEDW`).

The RGB and RGBW families are distinct types on purpose: a `WStripGroup` accepts only `WStrip`s, so mixing formats in one group is a compile error. Low-level free functions mirror the RGB ones — `fill(LEDW*, len, r, g, b, w)`, `fillFromTo(LEDW*, …)`, etc. RGBW uses the same signaling as WS2812B — verify the exact timing against your SK6812 batch.

## Async refresh (ESP32 / RP2040)

On ESP32/RP2040 the strip templates take an optional `bool ASYNC` (default `true`):

- `Strip<N>` / `WStrip<N>` — **async**: `show()` snapshots the pixels into a per-strip send buffer, starts the transfer and returns; you may edit the strip immediately. Call `wait()` before relying on completion, or `busy()` to poll. Different strips transmit concurrently. Costs one extra static buffer (`2 x pixels` bytes total).
- `Strip<N, false>` / `WStrip<N, false>` — **blocking**: no send buffer, `show()` returns when the frame is on the wire. Half the RAM, but the transfer reads the live buffer, so editing it from another task mid-`show()` needs your own mutex.

AVR/ESP8266 have no `ASYNC` parameter (they bit-bang with interrupts disabled, so `Strip<N,false>` there is a compile error) — `Strip<N>` is simply blocking.

## Installation

**Arduino IDE** — copy the repository into your `Arduino/libraries/` folder (or use *Sketch → Include Library → Add .ZIP Library*), then `#include <PixelPerfect.h>`.

**PlatformIO** — add the library to your `lib_deps`, or drop the repository into the project's `lib/` folder. Declare the target architecture(s) via the usual `platform`/`board` settings; the library manifest advertises `avr`, `esp32`, and `rp2040`.

**ESP-IDF** — see [ESP-IDF (without Arduino)](#esp-idf-without-arduino) below.

## Quick start

```cpp
#include <PixelPerfect.h>

using namespace pix;

Strip<39> strip{2}; // 39 pixels, data on pin 2

void setup() {
    strip.begin();            // configure the pin / acquire the driver
    strip.brightness = 120;   // 0..255, applied on every show()

    strip.fill(0, 255, 255);  // set all pixels to cyan
    strip.show();             // push the buffer to the strip

    delay(2000);

    strip.clear();            // set all pixels to off
    strip[10] = {255, 255, 0}; // address a single pixel
    strip[10].b = 255;        // or a single color channel
    strip.show(255);          // push once at full brightness, leaving `brightness` untouched
}

void loop() {}
```

## Core concepts

- **Color model.** A pixel is a `pix::LED` (aliased as `pix::Color`), a packed 3-byte value. You work with it as RGB — the constructor takes `(r, g, b)`, `operator[]` indexes `0=R, 1=G, 2=B`, and the 32-bit form is `0xRRGGBB`. The bytes are stored internally in the WS2812B wire order (GRB) so no reordering happens at refresh time; this is an implementation detail you can ignore.
- **Brightness.** Every strip has a public `brightness` field (0–255). `show()` scales each channel by it using a fast integer approximation, `channel * (brightness + 1) / 256`, so `255` is full output and `0` is off. `show(uint8_t)` lets you override the brightness for a single refresh without changing the stored value.
- **Reverse.** Constructing a strip with `reverse = true` flips the logical index order, so index `0` maps to the physically last pixel. Useful when a strip is mounted end-to-end with another.
- **Memory model.** `Strip<N>` stores its `N` pixels in a member array — no heap use, size known at compile time. `StripAllocate` stores them on the heap and can be resized at runtime with `changeLength()`.
- **Out-of-range access is safe.** Indexing a strip past its length returns a reference to a shared throwaway pixel instead of corrupting memory; writes to it have no visible effect.

---

# API Reference

All symbols live in the `pix` namespace.

## `pix::LED` (alias `pix::Color`)

A single pixel: three 8-bit channels, packed to exactly 3 bytes.

### Constructors

```cpp
LED();                                 // black (0, 0, 0)
LED(uint8_t r, uint8_t g, uint8_t b);  // from channels
LED(uint32_t color);                   // from a packed 0xRRGGBB value
```

| Parameter | Description |
| :-------- | :---------- |
| `r`, `g`, `b` | Red, green and blue channels (0–255). |
| `color` | Packed color as `0xRRGGBB`. |

The argument count is what selects the constructor: **three** numbers are read as channels (`LED{255, 0, 0}`), a **single** number as a packed color (`LED{0xFF0000}` is red). Note that this means a lone value is always the packed form — `LED{0x20}` is `0x000020` (blue = 32), not red. One- or two-argument construction is intentionally not provided, so that a single integer is never ambiguous between the two constructors.

### Operators

```cpp
explicit operator uint32_t() const;        // -> 0xRRGGBB  (explicit: use (uint32_t)pixel)
uint8_t& operator[](uint8_t index);        // 0=R, 1=G, 2=B
LED& operator=(uint32_t color);            // assign from 0xRRGGBB
LED& operator=(const LED& other);
bool operator==(const LED& other) const;
bool operator==(uint32_t color) const;
bool operator!=(const LED& other) const;
bool operator!=(uint32_t color) const;
LEDW toLEDW() const;                        // promote to RGBW, W = 0
```

`operator[]` returns a reference to the requested channel, so it can be read or written (`pixel[0] = 255;`). The index is reduced modulo 3, so `0`/`1`/`2` select R/G/B and any larger value wraps around rather than failing. The `uint32_t` conversion is **explicit** — write `uint32_t packed = (uint32_t)pixel;` — so an `LED` never silently turns into an integer (which used to clash with the RGBW overloads). The `LEDW` pixel mirrors this API with a fourth `W` channel and a `0xRRGGBBWW` packed form.

## Strips

All strip types derive from the abstract `pix::StripInterface` and therefore share the same API. Two concrete implementations are provided.

### Types

```cpp
// AVR / ESP8266:
template <uint16_t LEN>
using Strip = StripTemplate<LEN>;

// ESP32 / RP2040: an extra opt-out ASYNC parameter (see "Asynchronous refresh"):
template <uint16_t LEN, bool ASYNC = true>
using Strip = StripTemplate<LEN, ASYNC>;

class StripAllocate;                       // pixels stored on the heap, resizable
using StripHandler = StripInterface*;      // pointer to any strip, used by StripGroup
```

`Strip<LEN>` compiles everywhere. `Strip<LEN, false>` is accepted only on ESP32/RP2040 — the `ASYNC` parameter is deliberately not exposed on AVR/ESP8266 (see [Asynchronous refresh](#asynchronous-refresh-esp32--rp2040)).

### Constructors

```cpp
Strip<LEN>(uint8_t pin = 255, bool reverse = false);
Strip<LEN, false>(uint8_t pin = 255, bool reverse = false);   // ESP32/RP2040 only
StripAllocate(uint16_t pixelCount, uint8_t pin = 255, bool reverse = false);
StripAllocate();                           // empty; set length later with changeLength()
```

| Parameter | Description |
| :-------- | :---------- |
| `LEN` / `pixelCount` | Number of pixels in the strip. |
| `ASYNC` | *(ESP32/RP2040 only, default `true`.)* `true` = double-buffered, non-blocking `show()`; `false` = single-buffered, blocking `show()` (half the RAM). |
| `pin` | Data pin. `255` means "unassigned" — set it via the constructor or `setPin()` before calling `begin()`. |
| `reverse` | Reverse the logical pixel order. |

### Public fields

| Field | Type | Description |
| :---- | :--- | :---------- |
| `brightness` | `uint8_t` | Brightness applied by `show()`; defaults to `120`. |
| `reverse` | `bool` | Whether logical indexing is reversed. |

### Lifecycle

```cpp
void begin();
```
Prepares the strip for output: configures the data pin and, on ESP32, acquires and configures the RMT channel bound to that pin. Call it once in `setup()` / before the first `show()`.

### Addressing pixels

```cpp
LED& led(uint16_t i);
const LED& led(uint16_t i) const;
LED& operator[](uint16_t i);
const LED& operator[](uint16_t i) const;
```

Return a reference to pixel `i` (honoring `reverse`). `led(i)` and `operator[](i)` are equivalent. An index `>= length()` returns a shared dummy pixel, so out-of-range access neither crashes nor affects the strip.

### Setting colors

```cpp
void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
void fillFromTo(uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(const Color& color, uint16_t from, uint16_t to);
```

`fill(...)` sets every pixel to one color. `fillFromTo(...)` sets the inclusive range `[from, to]`. The range calls do nothing if `from > to` or `to >= length()`.

### Refreshing

```cpp
void show();                // scale by `brightness`, then transmit
void show(uint8_t bright);  // scale by `bright` for this refresh only

// ESP32 / RP2040 only:
void wait();                // block until this strip's async frame has finished
bool busy();                // true while an async frame is still on the wire
```

Transmits the pixel buffer to the physical strip. `show()` uses the strip's `brightness` field; `show(bright)` uses the supplied value without modifying the field.

On a default (async) `Strip<LEN>` on ESP32/RP2040, `show()` returns as soon as the transfer has started; use `wait()`/`busy()` to synchronize. On AVR/ESP8266, and on `Strip<LEN, false>`, `show()` blocks until the frame is fully sent. See [Asynchronous refresh](#asynchronous-refresh-esp32--rp2040).

### Other

```cpp
uint16_t length() const;    // pixel count
void clear();               // set every pixel to off (does not transmit)
void setPin(uint8_t p);
uint8_t getPin() const;
bool ok() const;             // ESP32 / RP2040: false if this pin has no channel
void changeLength(uint16_t l);   // StripAllocate only: reallocate to l pixels
```

`clear()` only updates the buffer; call `show()` afterwards to blank the physical strip. `ok()` reports whether this strip's pin currently has (or can still get) a hardware channel; on AVR/ESP8266, which have no channel concept, it is always `true`. See [Channel pooling and queueing](#channel-pooling-and-queueing-esp32--rp2040).

## Strip groups

`pix::StripGroup<N>` presents several independent strips as one continuous, zero-based index space, so an animation can span multiple data pins without tracking boundaries by hand. Each member strip keeps its own pixel buffer and is refreshed on its own pin.

```cpp
template <uint8_t STRIP_COUNT>
class StripGroup;                          // derives from StripGroupInterface

using StripGroupHandler = StripGroupInterface*;
```

### Constructors

```cpp
StripGroup();                                       // empty
StripGroup(const StripHandler (&strips)[STRIP_COUNT]); // from an array of strip pointers
```

Populate a group by passing an array of `StripHandler` (pointers to existing strips). The strips are referenced, not copied, so they must outlive the group.

```cpp
Strip<30> a{2};
Strip<30> b{4};
StripHandler handlers[2] = { &a, &b };
StripGroup<2> group{handlers};
```

### Public fields

| Field | Type | Description |
| :---- | :--- | :---------- |
| `brightness` | `uint8_t` | Brightness passed to each strip's `show()`; defaults to `128`. |

### Methods

```cpp
uint32_t length() const;        // cached total pixel count across all strips
uint32_t recalcLength();        // recompute and cache the total; call after populating the group
void begin();                   // call begin() on every member strip
void clear();                   // clear every member strip's buffer
void show();                    // refresh every strip using the group `brightness`
void show(uint8_t bright);      // refresh every strip using `bright`
void showStrip(uint8_t strip);  // refresh only the strip at the given slot

void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to);
void fillFromTo(uint32_t color, uint32_t from, uint32_t to);
void fillFromTo(const Color& color, uint32_t from, uint32_t to);

LED& led(uint32_t i);           // pixel by global index (spans all strips)
const LED& led(uint32_t i) const;
LED& operator[](uint32_t i);    // equivalent to led(i)
const LED& operator[](uint32_t i) const;

StripHandler strip(uint8_t s);  // pointer to the member strip in slot s (nullptr if out of range)
```

Call `recalcLength()` once after building the group (and again if you swap strips) so `length()`, `led()`, and the range fills know the total size. Indexing beyond the total returns the shared dummy pixel.

## Free functions (low-level)

These operate directly on a raw `LED*` buffer and are what the strip classes call internally. Use them when you manage the buffer yourself.

```cpp
void begin(uint8_t pin);
void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright = 255, bool scale = true);
void clear(LED* leds, uint16_t len);

void fill(LED* leds, uint16_t len, uint32_t color);
void fill(LED* leds, uint16_t len, const Color& color);
void fill(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b);
void fillFromTo(LED* leds, uint16_t len, uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, const Color& color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
```

| Parameter | Description |
| :-------- | :---------- |
| `leds` | Pointer to the pixel buffer. |
| `len` | Number of pixels in the buffer. |
| `pin` | Data pin. |
| `bright` | Brightness (0–255) applied when `scale` is `true`. |
| `scale` | If `false`, `show()` transmits the buffer verbatim with no brightness scaling. |
| `from`, `to` | Inclusive pixel range for `fillFromTo`. |

`begin(pin)` configures the pin (and, on ESP32, provisions the RMT channel). `show(...)` performs one transmission, enforcing the WS2812B inter-frame reset gap.

### Color helpers

```cpp
Color  hsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);
Color  kelvin(uint16_t temp);
ColorW hsvW(uint16_t hue, uint8_t sat = 255, uint8_t val = 255); // RGBW: LEDW, W = 0
ColorW kelvinW(uint16_t temp);                                   // RGBW: LEDW, W = 0
Color& gamma32(Color& color);   // gamma-correct in place, returns the same reference
uint32_t gamma32(uint32_t color); // gamma-correct a packed color
```

| Function | Description |
| :------- | :---------- |
| `hsv` | Convert HSV to RGB. `hue` covers the full color wheel over its whole `0–65535` range; `sat` and `val` are `0–255`. |
| `kelvin` | Approximate the RGB color of a black body at `temp` Kelvin (table covers roughly 520–10000 K). |
| `hsvW` / `kelvinW` | Same as `hsv` / `kelvin` but return an `LEDW` (`W = 0`), for use with `WStrip` without any cast. |
| `gamma32` | Apply perceptual gamma correction, either to a `Color` in place or to a `0xRRGGBB` value. |

---

## Asynchronous refresh (ESP32 / RP2040)

On ESP32 and RP2040 the transmission is driven by hardware (RMT / PIO+DMA), so it can run while the CPU does something else. This is exposed through the `ASYNC` template parameter, chosen per strip:

| | `Strip<LEN>` (`ASYNC = true`, default) | `Strip<LEN, false>` |
| :-- | :-- | :-- |
| Buffers | edit buffer **+** send buffer (snapshot) | edit buffer only |
| RAM per strip | `6 × LEN` bytes | `3 × LEN` bytes |
| `show()` | non-blocking: snapshots+scales into the send buffer, starts the transfer, returns | blocking: returns once the frame is on the wire |
| Multiple strips | transmit **concurrently** (each on its own channel) | one at a time |
| Editing pixels right after `show()` | always safe (transfer reads the snapshot) | safe from one task; concurrent tasks must be mutexed |

```cpp
Strip<60> a{2};     // async
Strip<60> b{4};     // async

a.fill(...); b.fill(...);
a.show();           // starts A, returns immediately
b.show();           // starts B while A is still on the wire -> both transmit at once
a.wait(); b.wait(); // block until each frame is done
```

Because `show()` snapshots into the send buffer, you may keep editing the strip's pixels straight after it returns without tearing the frame in flight. `wait()` before a second `show()` on the *same* strip is implicit (a strip finishes its previous frame before starting the next); different strips overlap freely. `busy()` polls without blocking. See `examples/async_multistrip`.

`ASYNC` is only available on ESP32/RP2040. On AVR/ESP8266 the parameter does not exist (`Strip<LEN, false>` is a compile error there): those bit-bang with interrupts disabled, so `show()` is always blocking and the frame cannot be torn.

### Channel pooling and queueing (ESP32 / RP2040)

Each live pin needs one hardware channel (an RMT channel on ESP32; a PIO state machine + DMA channel on RP2040), and every chip only has a handful of them: 8 on a classic ESP32, 4 on S2/S3, 2 TX on C3 (see `SOC_RMT_CHANNELS_PER_GROUP` for your chip); 8 on RP2040 (2 PIO blocks × 4 state machines). Declaring more strips than that is fine — what happens once you actually use them depends on the driver:

- **RP2040, and ESP32 on the legacy `driver/rmt.h` core (Arduino core 2.x / IDF 4):** channels are pooled and queued. A `show()`/async `show()` call gets the channel it already owns, or claims a free one, or — once the pool is full — steals the least-recently-used *idle* channel and rebinds it to the new pin. Rebinding is cheap (only the output GPIO changes; no channel or DMA claim is torn down) and never touches a channel that is still mid-transmission. If every channel happens to be busy at that instant, the call simply waits (yielding to other tasks) until one frees up — a brief, one-off stall rather than a failure.
- **ESP32 on the new `driver/rmt_tx.h` core (Arduino core 3.x / IDF 5+):** not pooled. This driver has no public API to rebind an existing channel to a different GPIO without deleting and recreating it (and its encoder) — too expensive to do transparently on every frame — so once all `SOC_RMT_CHANNELS_PER_GROUP` channels belong to other pins, further pins simply cannot get one. This is a limitation of Espressif's newer driver, not of this library: even FastLED works around it by reaching past the public RMT API into the GPIO matrix directly, a maintenance burden this library deliberately avoids.

Call `strip.ok()` to check whether a pin currently has (or can get) a channel. On IDF5 a `false` is durable (and also logged once per pin via `ESP_LOGE`) instead of the strip silently never lighting up; on RP2040/IDF4 a momentary `false` just means every channel is busy right now — the next `show()` will still succeed, only queued.

## Platform notes

### ESP32 and FreeRTOS

The RMT driver never disables interrupts or the scheduler while transmitting, so other tasks — and the second core — keep running normally. `examples/esp32_freertos_concurrency` demonstrates this: a second task keeps a steady cadence while a strip is refreshed in a tight loop.

Keep the following in mind:

- **No internal locking.** A given strip or group has no built-in synchronization. With an async `Strip<LEN>` you can freely edit pixels right after `show()`, but two tasks touching the *same* strip still need to be serialized. With `Strip<LEN, false>` (and on AVR/ESP8266) a concurrent write during `show()` can tear the frame — mutex it:

  ```cpp
  SemaphoreHandle_t stripLock = xSemaphoreCreateMutex();
  // ...
  xSemaphoreTake(stripLock, portMAX_DELAY);
  strip.fill(255, 0, 0);
  strip.show();
  xSemaphoreGive(stripLock);
  ```

- **Channel budget.** A classic ESP32 has 8 RMT channels (fewer on S2/S3/C3 — see `SOC_RMT_CHANNELS_PER_GROUP` for your chip). On the legacy core (IDF 4) more pins than that transparently share the pool via queueing; on the new core (IDF 5+) it is a hard cap. See [Channel pooling and queueing](#channel-pooling-and-queueing-esp32--rp2040).

Both arduino-esp32 core generations are supported and selected automatically: the legacy `driver/rmt.h` API (core 2.x / IDF 4) and the newer `driver/rmt_tx.h` encoder API (core 3.x / IDF 5). The library itself performs no dynamic allocation (the encoder/registry are static); the RMT SDK allocates its own channel structures once per pin at setup, which is inherent to using the driver.

### RP2040 / Raspberry Pi Pico

Targets the arduino-pico (Earle Philhower) core; a PIO state machine generates the WS2812B waveform in hardware, timed by the PIO so it needs no interrupt lock. Two transmission paths, both allocation-free (no heap at all):

- **Async `Strip<LEN>`:** a single DMA channel streams the pre-scaled send buffer into the PIO FIFO and returns; completion is polled in `wait()`/`busy()`. Up to 8 concurrently-active strips (2 PIO blocks × 4 state machines); beyond that, channels are pooled and queued — see [Channel pooling and queueing](#channel-pooling-and-queueing-esp32--rp2040).
- **Blocking (`Strip<LEN, false>` and the low-level `pix::show()`):** the CPU scales each byte and feeds the FIFO directly — no buffer, no DMA. The 8-deep TX FIFO (~80 µs) absorbs scheduling jitter.

### ESP8266

Targets the ESP8266 Arduino core. The waveform is bit-banged and timed against the Xtensa cycle counter (CCOUNT), derived from the live CPU frequency so it is correct at both 80 and 160 MHz. Any GPIO works (pin 16 included), unlike the I2S/UART tricks that lock you to a fixed pin. Brightness is scaled inline per byte, so nothing is allocated on a refresh.

Interrupts are disabled for the duration of a frame (the same trade-off as the AVR driver), because a mistimed interrupt would corrupt the signal. The core is single-threaded (non-FreeRTOS), so this does not stall a scheduler, but keep strips to a sensible length: interrupts stay off for roughly `pixels × 30 µs`, and holding them off for very long (thousands of pixels) can disturb Wi-Fi servicing.

### ESP-IDF (without Arduino)

The library also builds as a native **ESP-IDF component**, with no Arduino layer. It reuses the exact same RMT driver and `pix::` API; the few Arduino symbols the sources rely on (`micros`, `pinMode`/`digitalWrite`, `PROGMEM`/`pgm_read_byte`) are provided by `pix_platform.h`, which switches between `<Arduino.h>` and a pure-IDF shim based on whether `ARDUINO` and `ESP_PLATFORM` are defined.

Make the repository visible to your project as a component — either drop it under `components/PixelPerfect/`, or point `EXTRA_COMPONENT_DIRS` at it — then require it from your app component:

```cmake
idf_component_register(SRCS "app_main.cpp" INCLUDE_DIRS "." REQUIRES PixelPerfect)
```

Usage is identical to the Arduino API; only the entry point differs (`app_main()` instead of `setup()`/`loop()`):

```cpp
#include "PixelPerfect.h"
using namespace pix;

static Strip<39> strip{2};

extern "C" void app_main(void) {
    strip.begin();
    strip.brightness = 120;
    strip.fill(0, 255, 255);
    strip.show();
}
```

`examples/idf_basic` is a complete, buildable project (`idf.py build`).

> **Note:** on ESP-IDF ≥ 5.3 the RMT and GPIO drivers were split into separate components. If a build reports a missing `driver/rmt_tx.h` or `driver/gpio.h`, add `esp_driver_rmt esp_driver_gpio` to the component's `REQUIRES` in the root `CMakeLists.txt`.

## Compile-time configuration

| Macro | Effect |
| :---- | :----- |
| `DISABLE_GAMMA_LUT` | Compute gamma correction with floating-point math instead of the flash lookup table (smaller flash footprint, slower). |
| `DISABLE_KELVIN_LUT` | Compute `kelvin()` with floating-point math instead of the flash lookup table. |

## Examples

| Sketch | Platform | Shows |
| :----- | :------- | :---- |
| `examples/esp32_basic` | ESP32 (Arduino) | Minimal fill / show usage. |
| `examples/esp32_freertos_concurrency` | ESP32 (Arduino) | A refresh loop running alongside another FreeRTOS task without stalling it. |
| `examples/esp8266_basic` | ESP8266 (Arduino) | Minimal fill / show usage. |
| `examples/rp2040_basic` | RP2040 (Arduino) | Minimal fill / show usage. |
| `examples/rgbw_basic` | any (Arduino) | RGBW (`WStrip` / `LEDW`, SK6812) with the white channel. |
| `examples/async_multistrip` | ESP32 / RP2040 | Non-blocking `show()`, two strips transmitting at once, `wait()`, and a `Strip<N, false>` comparison. |
| `examples/idf_basic` | ESP32 (ESP-IDF) | The same API from a pure `app_main()` project. |

## License

See [LICENSE](LICENSE).

</details>

<details>
<summary><b>🇵🇱 Polski</b></summary>

<br>

Lekka biblioteka C++ do sterowania adresowalnymi taśmami LED **WS2812B** ("NeoPixel"). Ma być prostsza niż FastLED, unikając przy tym dynamicznej alokacji pamięci na gorącej ścieżce: domyślny typ taśmy trzyma bufor pikseli w stałej, statycznie rozmiarowanej tablicy.

Ten sam kod źródłowy kompiluje się bez zmian na kilku platformach, każda używając najlepszego mechanizmu wyjścia, jaki oferuje sprzęt:

| Platforma | Framework | Mechanizm wyjścia | Źródło |
| :-------- | :-------- | :---------------- | :----- |
| AVR (np. ATmega2560) | Arduino | Bit-banging liczony cyklami (wstawki asemblerowe) | `atmega_show.cpp` |
| ESP32 (wszystkie warianty) | Arduino **lub** ESP-IDF | Sprzętowe peryferium RMT | `esp32_show.cpp` |
| ESP8266 | Arduino | Bit-banging taktowany rejestrem CCOUNT | `esp8266_show.cpp` |
| RP2040 / Raspberry Pi Pico | Arduino (arduino-pico) | Maszyna stanów PIO + DMA | `rp2040_show.cpp` |

Właściwy backend jest wybierany automatycznie w czasie kompilacji na podstawie predefiniowanych makr frameworka — nigdy nie wybierasz go ręcznie.

## Autor

- [@syzyf97](https://www.github.com/syzyf97)

## Funkcje

- Taśma o stałym rozmiarze, bez alokacji (`Strip<N>`) oraz opcjonalny wariant na stercie (`StripAllocate`).
- **RGB i RGBW** (np. SK6812) jako dwie osobne, typo-bezpieczne rodziny: `Strip`/`LED` i `WStrip`/`LEDW`.
- **Nieblokujące odświeżanie** na ESP32/RP2040 (`Strip<N>` jest domyślnie asynchroniczny), więc kilka taśm może nadawać naraz; `wait()`/`busy()` do synchronizacji.
- Skalowanie jasności per taśma i per wywołanie.
- Wygodne helpery koloru: HSV, temperatura barwowa (Kelvin) i korekcja gamma, domyślnie oparte o tablice w pamięci flash.
- Grupowanie kilku taśm w jednej logicznej przestrzeni indeksów (`StripGroup` / `WStripGroup`).
- Przyjazne FreeRTOS na ESP32 i RP2040: odświeżanie nigdy nie blokuje schedulera ani drugiego rdzenia.

## Formaty koloru: RGB i RGBW

Dwie równoległe, typo-bezpieczne rodziny współdzielą jedną implementację:

| | Piksel | Taśma | Grupa | Wariant stertowy |
| :-- | :-- | :-- | :-- | :-- |
| **RGB** (WS2812B) | `LED` (3 B) | `Strip<N>` | `StripGroup<M>` | `StripAllocate` |
| **RGBW** (SK6812) | `LEDW` (4 B) | `WStrip<N>` | `WStripGroup<M>` | `WStripAllocate` |

API jest identyczne; RGBW dodaje tylko kanał bieli:

```cpp
WStrip<30> strip{2};
strip.begin();
strip.fill(0, 0, 0, 255);    // pure white through the W channel
strip.fill(255, 0, 0);       // red; the 3-argument fill sets W = 0
strip[10] = {0, 0, 255, 80}; // per-pixel R,G,B,W
strip[11] = hsvW(1000);      // RGBW color helpers -> LEDW (W = 0)
strip.fill(kelvinW(3000));   // no cast, no ambiguity
```

`LEDW` to spakowana 4-bajtowa wartość; jej postać 32-bitowa to `0xRRGGBBWW`, a `operator[]` indeksuje `0=R, 1=G, 2=B, 3=W`. Używaj helperów RGBW `hsvW()` / `kelvinW()` — zwracają `LEDW` (z `W = 0`), więc kod RGBW nigdy nie dotyka typu `LED`. Aby świadomie awansować istniejący `LED`, wywołaj `led.toLEDW()` (nie ma niejawnej konwersji RGB→RGBW: `strip[i] = hsv(x)` to celowy błąd kompilacji kierujący do `hsvW` / `toLEDW`).

Rodziny RGB i RGBW to celowo różne typy: `WStripGroup` przyjmuje wyłącznie taśmy `WStrip`, więc mieszanie formatów w jednej grupie to błąd kompilacji. Niskopoziomowe wolne funkcje odzwierciedlają te z RGB — `fill(LEDW*, len, r, g, b, w)`, `fillFromTo(LEDW*, …)` itd. RGBW używa tej samej sygnalizacji co WS2812B — sprawdź dokładny timing na swojej partii SK6812.

## Odświeżanie asynchroniczne (ESP32 / RP2040)

Na ESP32/RP2040 szablony taśm przyjmują opcjonalny `bool ASYNC` (domyślnie `true`):

- `Strip<N>` / `WStrip<N>` — **async**: `show()` robi migawkę pikseli do bufora wysyłkowego per taśma, startuje transfer i wraca; możesz od razu edytować taśmę. Wywołaj `wait()`, zanim polegniesz na zakończeniu, albo `busy()` do odpytywania. Różne taśmy nadają współbieżnie. Kosztuje jeden dodatkowy bufor statyczny (`2 x piksele` bajtów łącznie).
- `Strip<N, false>` / `WStrip<N, false>` — **blokujące**: bez bufora wysyłkowego, `show()` wraca, gdy ramka jest już na przewodzie. Połowa RAM, ale transfer czyta żywy bufor, więc edycja z innego zadania w trakcie `show()` wymaga własnego muteksa.

AVR/ESP8266 nie mają parametru `ASYNC` (bit-bangują z wyłączonymi przerwaniami, więc `Strip<N,false>` jest tam błędem kompilacji) — `Strip<N>` jest po prostu blokujący.

## Instalacja

**Arduino IDE** — skopiuj repozytorium do folderu `Arduino/libraries/` (lub użyj *Sketch → Include Library → Add .ZIP Library*), następnie `#include <PixelPerfect.h>`.

**PlatformIO** — dodaj bibliotekę do `lib_deps` albo wrzuć repozytorium do folderu `lib/` projektu. Zadeklaruj docelowe architektury przez zwykłe ustawienia `platform`/`board`; manifest biblioteki ogłasza `avr`, `esp32` i `rp2040`.

**ESP-IDF** — patrz **ESP-IDF (bez Arduino)** poniżej.

## Szybki start

```cpp
#include <PixelPerfect.h>

using namespace pix;

Strip<39> strip{2}; // 39 pixels, data on pin 2

void setup() {
    strip.begin();            // configure the pin / acquire the driver
    strip.brightness = 120;   // 0..255, applied on every show()

    strip.fill(0, 255, 255);  // set all pixels to cyan
    strip.show();             // push the buffer to the strip

    delay(2000);

    strip.clear();            // set all pixels to off
    strip[10] = {255, 255, 0}; // address a single pixel
    strip[10].b = 255;        // or a single color channel
    strip.show(255);          // push once at full brightness, leaving `brightness` untouched
}

void loop() {}
```

## Podstawowe pojęcia

- **Model koloru.** Piksel to `pix::LED` (alias `pix::Color`), spakowana 3-bajtowa wartość. Pracujesz z nim jak z RGB — konstruktor przyjmuje `(r, g, b)`, `operator[]` indeksuje `0=R, 1=G, 2=B`, a postać 32-bitowa to `0xRRGGBB`. Bajty są przechowywane wewnętrznie w kolejności przewodu WS2812B (GRB), więc przy odświeżaniu nie ma przestawiania; to szczegół implementacyjny, który możesz zignorować.
- **Jasność.** Każda taśma ma publiczne pole `brightness` (0–255). `show()` skaluje każdy kanał szybkim przybliżeniem całkowitoliczbowym, `kanał * (brightness + 1) / 256`, więc `255` to pełne wyjście, a `0` to zgaszenie. `show(uint8_t)` pozwala nadpisać jasność na pojedyncze odświeżenie bez zmiany zapisanej wartości.
- **Reverse.** Utworzenie taśmy z `reverse = true` odwraca logiczną kolejność indeksów, więc indeks `0` mapuje się na fizycznie ostatni piksel. Przydatne, gdy taśma jest zamontowana koniec-do-końca z inną.
- **Model pamięci.** `Strip<N>` trzyma swoje `N` pikseli w tablicy składowej — bez sterty, rozmiar znany w czasie kompilacji. `StripAllocate` trzyma je na stercie i można zmienić rozmiar w czasie działania przez `changeLength()`.
- **Dostęp poza zakresem jest bezpieczny.** Indeksowanie taśmy poza jej długość zwraca referencję do współdzielonego piksela-atrapy zamiast psuć pamięć; zapisy do niego nie mają widocznego efektu.

---

# Dokumentacja API

Wszystkie symbole żyją w przestrzeni nazw `pix`.

## `pix::LED` (alias `pix::Color`)

Pojedynczy piksel: trzy 8-bitowe kanały, spakowane do dokładnie 3 bajtów.

### Konstruktory

```cpp
LED();                                 // black (0, 0, 0)
LED(uint8_t r, uint8_t g, uint8_t b);  // from channels
LED(uint32_t color);                   // from a packed 0xRRGGBB value
```

| Parametr | Opis |
| :-------- | :--- |
| `r`, `g`, `b` | Kanały czerwony, zielony i niebieski (0–255). |
| `color` | Spakowany kolor jako `0xRRGGBB`. |

Liczba argumentów decyduje o wyborze konstruktora: **trzy** liczby są czytane jako kanały (`LED{255, 0, 0}`), **pojedyncza** liczba jako spakowany kolor (`LED{0xFF0000}` to czerwień). Oznacza to, że pojedyncza wartość jest zawsze formą spakowaną — `LED{0x20}` to `0x000020` (niebieski = 32), a nie czerwony. Konstrukcja jedno- lub dwuargumentowa celowo nie jest udostępniona, żeby pojedyncza liczba całkowita nigdy nie była niejednoznaczna między dwoma konstruktorami.

### Operatory

```cpp
explicit operator uint32_t() const;        // -> 0xRRGGBB  (explicit: use (uint32_t)pixel)
uint8_t& operator[](uint8_t index);        // 0=R, 1=G, 2=B
LED& operator=(uint32_t color);            // assign from 0xRRGGBB
LED& operator=(const LED& other);
bool operator==(const LED& other) const;
bool operator==(uint32_t color) const;
bool operator!=(const LED& other) const;
bool operator!=(uint32_t color) const;
LEDW toLEDW() const;                        // promote to RGBW, W = 0
```

`operator[]` zwraca referencję do żądanego kanału, więc można go czytać lub zapisywać (`pixel[0] = 255;`). Indeks jest redukowany modulo 3, więc `0`/`1`/`2` wybierają R/G/B, a każda większa wartość zawija się zamiast zawodzić. Konwersja do `uint32_t` jest **jawna** — pisz `uint32_t packed = (uint32_t)pixel;` — więc `LED` nigdy nie zamienia się po cichu w liczbę (co kiedyś kolidowało z przeciążeniami RGBW). Piksel `LEDW` odzwierciedla to API z czwartym kanałem `W` i formą spakowaną `0xRRGGBBWW`.

## Taśmy

Wszystkie typy taśm dziedziczą po abstrakcyjnym `pix::StripInterface`, więc mają to samo API. Dostępne są dwie konkretne implementacje.

### Typy

```cpp
// AVR / ESP8266:
template <uint16_t LEN>
using Strip = StripTemplate<LEN>;

// ESP32 / RP2040: an extra opt-out ASYNC parameter (see "Asynchronous refresh"):
template <uint16_t LEN, bool ASYNC = true>
using Strip = StripTemplate<LEN, ASYNC>;

class StripAllocate;                       // pixels stored on the heap, resizable
using StripHandler = StripInterface*;      // pointer to any strip, used by StripGroup
```

`Strip<LEN>` kompiluje się wszędzie. `Strip<LEN, false>` jest akceptowany tylko na ESP32/RP2040 — parametr `ASYNC` celowo nie jest udostępniony na AVR/ESP8266 (patrz **Odświeżanie asynchroniczne**).

### Konstruktory

```cpp
Strip<LEN>(uint8_t pin = 255, bool reverse = false);
Strip<LEN, false>(uint8_t pin = 255, bool reverse = false);   // ESP32/RP2040 only
StripAllocate(uint16_t pixelCount, uint8_t pin = 255, bool reverse = false);
StripAllocate();                           // empty; set length later with changeLength()
```

| Parametr | Opis |
| :-------- | :--- |
| `LEN` / `pixelCount` | Liczba pikseli w taśmie. |
| `ASYNC` | *(tylko ESP32/RP2040, domyślnie `true`.)* `true` = podwójny bufor, nieblokujące `show()`; `false` = pojedynczy bufor, blokujące `show()` (połowa RAM). |
| `pin` | Pin danych. `255` oznacza "nieprzypisany" — ustaw przez konstruktor lub `setPin()` przed wywołaniem `begin()`. |
| `reverse` | Odwróć logiczną kolejność pikseli. |

### Pola publiczne

| Pole | Typ | Opis |
| :--- | :-- | :--- |
| `brightness` | `uint8_t` | Jasność stosowana przez `show()`; domyślnie `120`. |
| `reverse` | `bool` | Czy logiczne indeksowanie jest odwrócone. |

### Cykl życia

```cpp
void begin();
```
Przygotowuje taśmę do wyjścia: konfiguruje pin danych i, na ESP32, pozyskuje oraz konfiguruje kanał RMT powiązany z tym pinem. Wywołaj raz w `setup()` / przed pierwszym `show()`.

### Adresowanie pikseli

```cpp
LED& led(uint16_t i);
const LED& led(uint16_t i) const;
LED& operator[](uint16_t i);
const LED& operator[](uint16_t i) const;
```

Zwracają referencję do piksela `i` (uwzględniając `reverse`). `led(i)` i `operator[](i)` są równoważne. Indeks `>= length()` zwraca współdzielony piksel-atrapę, więc dostęp poza zakresem ani się nie wywala, ani nie wpływa na taśmę.

### Ustawianie kolorów

```cpp
void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
void fillFromTo(uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(const Color& color, uint16_t from, uint16_t to);
```

`fill(...)` ustawia każdy piksel na jeden kolor. `fillFromTo(...)` ustawia domknięty zakres `[from, to]`. Wywołania zakresowe nic nie robią, jeśli `from > to` lub `to >= length()`.

### Odświeżanie

```cpp
void show();                // scale by `brightness`, then transmit
void show(uint8_t bright);  // scale by `bright` for this refresh only

// ESP32 / RP2040 only:
void wait();                // block until this strip's async frame has finished
bool busy();                // true while an async frame is still on the wire
```

Wysyła bufor pikseli do fizycznej taśmy. `show()` używa pola `brightness` taśmy; `show(bright)` używa podanej wartości bez zmiany pola.

Na domyślnej (async) `Strip<LEN>` na ESP32/RP2040 `show()` wraca, gdy tylko transfer wystartował; użyj `wait()`/`busy()` do synchronizacji. Na AVR/ESP8266 oraz na `Strip<LEN, false>` `show()` blokuje aż ramka zostanie w pełni wysłana. Patrz **Odświeżanie asynchroniczne**.

### Pozostałe

```cpp
uint16_t length() const;    // pixel count
void clear();               // set every pixel to off (does not transmit)
void setPin(uint8_t p);
uint8_t getPin() const;
bool ok() const;             // ESP32 / RP2040: false, jeśli ten pin nie ma kanału
void changeLength(uint16_t l);   // StripAllocate only: reallocate to l pixels
```

`clear()` aktualizuje tylko bufor; wywołaj potem `show()`, aby zgasić fizyczną taśmę. `ok()` mówi, czy pin tej taśmy aktualnie ma (albo wciąż może dostać) kanał sprzętowy; na AVR/ESP8266, które nie mają pojęcia kanału, zawsze zwraca `true`. Patrz **Pula i kolejkowanie kanałów**.

## Grupy taśm

`pix::StripGroup<N>` przedstawia kilka niezależnych taśm jako jedną ciągłą przestrzeń indeksów liczoną od zera, więc animacja może rozciągać się na wiele pinów danych bez ręcznego pilnowania granic. Każda taśma-składnik trzyma własny bufor pikseli i jest odświeżana na własnym pinie.

```cpp
template <uint8_t STRIP_COUNT>
class StripGroup;                          // derives from StripGroupInterface

using StripGroupHandler = StripGroupInterface*;
```

### Konstruktory

```cpp
StripGroup();                                       // empty
StripGroup(const StripHandler (&strips)[STRIP_COUNT]); // from an array of strip pointers
```

Wypełnij grupę, przekazując tablicę `StripHandler` (wskaźników do istniejących taśm). Taśmy są referencjonowane, nie kopiowane, więc muszą przeżyć grupę.

```cpp
Strip<30> a{2};
Strip<30> b{4};
StripHandler handlers[2] = { &a, &b };
StripGroup<2> group{handlers};
```

### Pola publiczne

| Pole | Typ | Opis |
| :--- | :-- | :--- |
| `brightness` | `uint8_t` | Jasność przekazywana do `show()` każdej taśmy; domyślnie `128`. |

### Metody

```cpp
uint32_t length() const;        // cached total pixel count across all strips
uint32_t recalcLength();        // recompute and cache the total; call after populating the group
void begin();                   // call begin() on every member strip
void clear();                   // clear every member strip's buffer
void show();                    // refresh every strip using the group `brightness`
void show(uint8_t bright);      // refresh every strip using `bright`
void showStrip(uint8_t strip);  // refresh only the strip at the given slot

void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to);
void fillFromTo(uint32_t color, uint32_t from, uint32_t to);
void fillFromTo(const Color& color, uint32_t from, uint32_t to);

LED& led(uint32_t i);           // pixel by global index (spans all strips)
const LED& led(uint32_t i) const;
LED& operator[](uint32_t i);    // equivalent to led(i)
const LED& operator[](uint32_t i) const;

StripHandler strip(uint8_t s);  // pointer to the member strip in slot s (nullptr if out of range)
```

Wywołaj `recalcLength()` raz po zbudowaniu grupy (i ponownie, jeśli wymienisz taśmy), aby `length()`, `led()` i wypełnienia zakresowe znały łączny rozmiar. Indeksowanie poza łączny rozmiar zwraca współdzielony piksel-atrapę.

## Wolne funkcje (niskopoziomowe)

Operują bezpośrednio na surowym buforze `LED*` i to je klasy taśm wołają wewnętrznie. Używaj ich, gdy sam zarządzasz buforem.

```cpp
void begin(uint8_t pin);
void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright = 255, bool scale = true);
void clear(LED* leds, uint16_t len);

void fill(LED* leds, uint16_t len, uint32_t color);
void fill(LED* leds, uint16_t len, const Color& color);
void fill(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b);
void fillFromTo(LED* leds, uint16_t len, uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, const Color& color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
```

| Parametr | Opis |
| :-------- | :--- |
| `leds` | Wskaźnik na bufor pikseli. |
| `len` | Liczba pikseli w buforze. |
| `pin` | Pin danych. |
| `bright` | Jasność (0–255) stosowana, gdy `scale` jest `true`. |
| `scale` | Jeśli `false`, `show()` wysyła bufor dosłownie, bez skalowania jasności. |
| `from`, `to` | Domknięty zakres pikseli dla `fillFromTo`. |

`begin(pin)` konfiguruje pin (i, na ESP32, zakłada kanał RMT). `show(...)` wykonuje jedną transmisję, wymuszając przerwę resetu WS2812B między ramkami.

### Helpery koloru

```cpp
Color  hsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);
Color  kelvin(uint16_t temp);
ColorW hsvW(uint16_t hue, uint8_t sat = 255, uint8_t val = 255); // RGBW: LEDW, W = 0
ColorW kelvinW(uint16_t temp);                                   // RGBW: LEDW, W = 0
Color& gamma32(Color& color);   // gamma-correct in place, returns the same reference
uint32_t gamma32(uint32_t color); // gamma-correct a packed color
```

| Funkcja | Opis |
| :------ | :--- |
| `hsv` | Konwertuje HSV na RGB. `hue` pokrywa pełne koło barw na całym zakresie `0–65535`; `sat` i `val` to `0–255`. |
| `kelvin` | Przybliża kolor RGB ciała doskonale czarnego w temperaturze `temp` w kelwinach (tablica pokrywa ok. 520–10000 K). |
| `hsvW` / `kelvinW` | To samo co `hsv` / `kelvin`, ale zwracają `LEDW` (`W = 0`), do użycia z `WStrip` bez żadnego rzutowania. |
| `gamma32` | Stosuje percepcyjną korekcję gamma — do `Color` w miejscu albo do wartości `0xRRGGBB`. |

---

## Odświeżanie asynchroniczne (ESP32 / RP2040)

Na ESP32 i RP2040 transmisję prowadzi sprzęt (RMT / PIO+DMA), więc może działać, gdy CPU robi coś innego. Jest to wystawione przez parametr szablonu `ASYNC`, wybierany per taśma:

| | `Strip<LEN>` (`ASYNC = true`, domyślnie) | `Strip<LEN, false>` |
| :-- | :-- | :-- |
| Bufory | bufor edycji **+** bufor wysyłkowy (migawka) | tylko bufor edycji |
| RAM na taśmę | `6 × LEN` bajtów | `3 × LEN` bajtów |
| `show()` | nieblokujące: robi migawkę+skaluje do bufora wysyłkowego, startuje transfer, wraca | blokujące: wraca, gdy ramka jest na przewodzie |
| Wiele taśm | nadają **współbieżnie** (każda na własnym kanale) | jedna po drugiej |
| Edycja pikseli zaraz po `show()` | zawsze bezpieczna (transfer czyta migawkę) | bezpieczna z jednego zadania; zadania współbieżne trzeba mutexować |

```cpp
Strip<60> a{2};     // async
Strip<60> b{4};     // async

a.fill(...); b.fill(...);
a.show();           // starts A, returns immediately
b.show();           // starts B while A is still on the wire -> both transmit at once
a.wait(); b.wait(); // block until each frame is done
```

Ponieważ `show()` robi migawkę do bufora wysyłkowego, możesz dalej edytować piksele taśmy zaraz po jego powrocie, bez rozrywania ramki w locie. `wait()` przed drugim `show()` na *tej samej* taśmie jest niejawne (taśma kończy poprzednią ramkę przed startem następnej); różne taśmy nakładają się swobodnie. `busy()` odpytuje bez blokowania. Patrz `examples/async_multistrip`.

`ASYNC` jest dostępny tylko na ESP32/RP2040. Na AVR/ESP8266 parametr nie istnieje (`Strip<LEN, false>` jest tam błędem kompilacji): bit-bangują z wyłączonymi przerwaniami, więc `show()` jest zawsze blokujące i ramki nie da się rozerwać.

### Pula i kolejkowanie kanałów (ESP32 / RP2040)

Każdy żywy pin potrzebuje jednego kanału sprzętowego (kanał RMT na ESP32; maszyna stanów PIO + kanał DMA na RP2040), a każdy chip ma ich tylko garść: 8 na klasycznym ESP32, 4 na S2/S3, 2 TX na C3 (patrz `SOC_RMT_CHANNELS_PER_GROUP` dla swojego chipu); 8 na RP2040 (2 bloki PIO × 4 maszyny stanów). Zadeklarowanie więcej taśm niż to jest w porządku — to, co się dzieje, gdy faktycznie ich użyjesz, zależy od sterownika:

- **RP2040 oraz ESP32 na starszym rdzeniu `driver/rmt.h` (rdzeń Arduino 2.x / IDF 4):** kanały są łączone w pulę i kolejkowane. Wywołanie `show()`/asynchronicznego `show()` dostaje kanał, który już posiada, albo zajmuje wolny, albo — gdy pula jest pełna — kradnie najdawniej używany **bezczynny** kanał i przepina go na nowy pin. Przepięcie jest tanie (zmienia się tylko wyjściowy GPIO; żaden kanał ani DMA nie jest niszczony) i nigdy nie dotyka kanału w trakcie transmisji. Jeśli akurat wszystkie kanały są zajęte, wywołanie po prostu czeka (oddając czas innym zadaniom), aż któryś się zwolni — to krótki, jednorazowy przestój, nie błąd.
- **ESP32 na nowym rdzeniu `driver/rmt_tx.h` (rdzeń Arduino 3.x / IDF 5+):** brak puli. Ten sterownik nie ma publicznego API do przepięcia istniejącego kanału na inny GPIO bez jego usunięcia i odtworzenia (razem z enkoderem) — zbyt drogie, by robić to przezroczyście przy każdej ramce — więc gdy wszystkie kanały `SOC_RMT_CHANNELS_PER_GROUP` należą już do innych pinów, kolejne piny po prostu nie mogą go dostać. To ograniczenie nowszego sterownika Espressif, nie tej biblioteki: nawet FastLED obchodzi to, sięgając poza publiczne API RMT bezpośrednio do GPIO matrix — obciążenie utrzymaniowe, którego ta biblioteka świadomie unika.

Wywołaj `strip.ok()`, aby sprawdzić, czy pin aktualnie ma (albo może dostać) kanał. Na IDF5 `false` jest trwałe (i dodatkowo logowane raz na pin przez `ESP_LOGE`) zamiast tego, żeby taśma po cichu nigdy się nie zaświeciła; na RP2040/IDF4 chwilowe `false` oznacza tylko, że wszystkie kanały są akurat zajęte — kolejne `show()` i tak się powiedzie, tylko zakolejkowane.

## Uwagi o platformach

### ESP32 i FreeRTOS

Sterownik RMT nigdy nie wyłącza przerwań ani schedulera podczas transmisji, więc inne zadania — i drugi rdzeń — działają normalnie. `examples/esp32_freertos_concurrency` to pokazuje: drugie zadanie trzyma równy rytm, gdy taśma jest odświeżana w ciasnej pętli.

Miej na uwadze:

- **Brak wewnętrznego blokowania.** Dana taśma lub grupa nie ma wbudowanej synchronizacji. Przy async `Strip<LEN>` możesz swobodnie edytować piksele zaraz po `show()`, ale dwa zadania dotykające *tej samej* taśmy nadal trzeba serializować. Przy `Strip<LEN, false>` (i na AVR/ESP8266) równoległy zapis podczas `show()` może rozerwać ramkę — użyj muteksa:

  ```cpp
  SemaphoreHandle_t stripLock = xSemaphoreCreateMutex();
  // ...
  xSemaphoreTake(stripLock, portMAX_DELAY);
  strip.fill(255, 0, 0);
  strip.show();
  xSemaphoreGive(stripLock);
  ```

- **Budżet kanałów.** Klasyczny ESP32 ma 8 kanałów RMT (mniej na S2/S3/C3 — patrz `SOC_RMT_CHANNELS_PER_GROUP` dla swojego chipu). Na starszym rdzeniu (IDF 4) więcej pinów niż to przezroczyście dzieli pulę przez kolejkowanie; na nowym rdzeniu (IDF 5+) to twardy limit. Patrz **Pula i kolejkowanie kanałów**.

Obie generacje rdzenia arduino-esp32 są wspierane i wybierane automatycznie: starsze API `driver/rmt.h` (rdzeń 2.x / IDF 4) i nowsze API enkodera `driver/rmt_tx.h` (rdzeń 3.x / IDF 5). Sama biblioteka nie robi alokacji dynamicznej (enkoder/rejestr są statyczne); SDK RMT alokuje własne struktury kanału raz na pin przy zakładaniu, co jest nieodłączne od użycia sterownika.

### RP2040 / Raspberry Pi Pico

Celuje w rdzeń arduino-pico (Earle Philhower); maszyna stanów PIO generuje przebieg WS2812B sprzętowo, taktowana przez PIO, więc nie potrzebuje blokady przerwań. Dwie ścieżki transmisji, obie bez alokacji (w ogóle bez sterty):

- **Async `Strip<LEN>`:** pojedynczy kanał DMA strumieniuje przeskalowany bufor wysyłkowy do FIFO PIO i wraca; zakończenie odpytywane w `wait()`/`busy()`. Do 8 jednocześnie aktywnych taśm (2 bloki PIO × 4 maszyny stanów); powyżej tego kanały są łączone w pulę i kolejkowane — patrz **Pula i kolejkowanie kanałów**.
- **Blokujące (`Strip<LEN, false>` i niskopoziomowe `pix::show()`):** CPU skaluje każdy bajt i karmi FIFO bezpośrednio — bez bufora, bez DMA. 8-elementowe FIFO TX (~80 µs) pochłania jitter szeregowania.

### ESP8266

Celuje w rdzeń Arduino dla ESP8266. Przebieg jest bit-bangowany i taktowany rejestrem cykli Xtensa (CCOUNT), wyliczanym z żywej częstotliwości CPU, więc jest poprawny i przy 80, i przy 160 MHz. Działa dowolny GPIO (łącznie z pinem 16), w przeciwieństwie do sztuczek I2S/UART, które przypinają do stałego pinu. Jasność skalowana w locie per bajt, więc przy odświeżeniu nic nie jest alokowane.

Przerwania są wyłączone na czas ramki (ten sam kompromis co w sterowniku AVR), bo źle taktowane przerwanie zepsułoby sygnał. Rdzeń jest jednowątkowy (nie FreeRTOS), więc nie blokuje to schedulera, ale trzymaj rozsądną długość taśm: przerwania są wyłączone przez ok. `piksele × 30 µs`, a trzymanie ich wyłączonych bardzo długo (tysiące pikseli) może zakłócić obsługę Wi-Fi.

### ESP-IDF (bez Arduino)

Biblioteka buduje się też jako natywny **komponent ESP-IDF**, bez warstwy Arduino. Reużywa dokładnie tego samego sterownika RMT i API `pix::`; nieliczne symbole Arduino, na których polega kod (`micros`, `pinMode`/`digitalWrite`, `PROGMEM`/`pgm_read_byte`), są dostarczane przez `pix_platform.h`, który przełącza się między `<Arduino.h>` a shimem czysto-IDF w zależności od tego, czy zdefiniowane są `ARDUINO` i `ESP_PLATFORM`.

Udostępnij repozytorium projektowi jako komponent — albo wrzuć je pod `components/PixelPerfect/`, albo wskaż na nie `EXTRA_COMPONENT_DIRS` — potem wymagaj go z komponentu aplikacji:

```cmake
idf_component_register(SRCS "app_main.cpp" INCLUDE_DIRS "." REQUIRES PixelPerfect)
```

Użycie jest identyczne z API Arduino; różni się tylko punkt wejścia (`app_main()` zamiast `setup()`/`loop()`):

```cpp
#include "PixelPerfect.h"
using namespace pix;

static Strip<39> strip{2};

extern "C" void app_main(void) {
    strip.begin();
    strip.brightness = 120;
    strip.fill(0, 255, 255);
    strip.show();
}
```

`examples/idf_basic` to kompletny, budowalny projekt (`idf.py build`).

> **Uwaga:** na ESP-IDF ≥ 5.3 sterowniki RMT i GPIO zostały rozdzielone na osobne komponenty. Jeśli build zgłosi brak `driver/rmt_tx.h` lub `driver/gpio.h`, dodaj `esp_driver_rmt esp_driver_gpio` do `REQUIRES` komponentu w głównym `CMakeLists.txt`.

## Konfiguracja w czasie kompilacji

| Makro | Efekt |
| :---- | :---- |
| `DISABLE_GAMMA_LUT` | Liczy korekcję gamma matematyką zmiennoprzecinkową zamiast tablicy we flashu (mniejszy ślad we flashu, wolniej). |
| `DISABLE_KELVIN_LUT` | Liczy `kelvin()` matematyką zmiennoprzecinkową zamiast tablicy we flashu. |

## Przykłady

| Szkic | Platforma | Pokazuje |
| :---- | :-------- | :------- |
| `examples/esp32_basic` | ESP32 (Arduino) | Minimalne użycie fill / show. |
| `examples/esp32_freertos_concurrency` | ESP32 (Arduino) | Pętla odświeżania obok innego zadania FreeRTOS bez zacinania go. |
| `examples/esp8266_basic` | ESP8266 (Arduino) | Minimalne użycie fill / show. |
| `examples/rp2040_basic` | RP2040 (Arduino) | Minimalne użycie fill / show. |
| `examples/rgbw_basic` | dowolna (Arduino) | RGBW (`WStrip` / `LEDW`, SK6812) z kanałem bieli. |
| `examples/async_multistrip` | ESP32 / RP2040 | Nieblokujące `show()`, dwie taśmy nadające naraz, `wait()` i porównanie z `Strip<N, false>`. |
| `examples/idf_basic` | ESP32 (ESP-IDF) | To samo API z czystego projektu `app_main()`. |

## Licencja

Patrz [LICENSE](LICENSE).

</details>

<details>
<summary><b>🇩🇪 Deutsch</b></summary>

<br>

Eine schlanke C++-Bibliothek zum Ansteuern adressierbarer **WS2812B**-LED-Streifen ("NeoPixel"). Sie will einfacher sein als FastLED und vermeidet dabei dynamische Speicherzuteilung auf dem heißen Pfad: der Standard-Streifentyp hält seinen Pixelpuffer in einem festen, statisch dimensionierten Array.

Derselbe Quellcode baut unverändert auf mehreren Zielplattformen, jede nutzt den besten Ausgabemechanismus der Hardware:

| Plattform | Framework | Ausgabemechanismus | Quelle |
| :-------- | :-------- | :----------------- | :----- |
| AVR (z. B. ATmega2560) | Arduino | Zyklusgezähltes Bit-Banging (Inline-Assembler) | `atmega_show.cpp` |
| ESP32 (alle Varianten) | Arduino **oder** ESP-IDF | Hardware-RMT-Peripherie | `esp32_show.cpp` |
| ESP8266 | Arduino | CCOUNT-getaktetes Bit-Banging | `esp8266_show.cpp` |
| RP2040 / Raspberry Pi Pico | Arduino (arduino-pico) | PIO-Zustandsmaschine + DMA | `rp2040_show.cpp` |

Das richtige Backend wird zur Kompilierzeit automatisch anhand vordefinierter Framework-Makros gewählt — du wählst es nie von Hand.

## Autor

- [@syzyf97](https://www.github.com/syzyf97)

## Funktionen

- Streifen fester Größe ohne Allokation (`Strip<N>`) plus optionale Heap-Variante (`StripAllocate`).
- **RGB und RGBW** (z. B. SK6812) als zwei getrennte, typsichere Familien: `Strip`/`LED` und `WStrip`/`LEDW`.
- **Nicht blockierendes Refresh** auf ESP32/RP2040 (`Strip<N>` ist standardmäßig asynchron), sodass mehrere Streifen gleichzeitig senden; `wait()`/`busy()` zur Synchronisation.
- Helligkeitsskalierung pro Streifen und pro Aufruf.
- Praktische Farb-Helfer: HSV, Farbtemperatur (Kelvin) und Gammakorrektur, standardmäßig über Lookup-Tabellen im Flash.
- Gruppierung mehrerer Streifen in einem logischen Indexraum (`StripGroup` / `WStripGroup`).
- FreeRTOS-freundlich auf ESP32 und RP2040: ein Refresh blockiert nie den Scheduler oder den zweiten Kern.

## Farbformate: RGB und RGBW

Zwei parallele, typsichere Familien teilen sich eine Implementierung:

| | Pixel | Streifen | Gruppe | Heap-Variante |
| :-- | :-- | :-- | :-- | :-- |
| **RGB** (WS2812B) | `LED` (3 B) | `Strip<N>` | `StripGroup<M>` | `StripAllocate` |
| **RGBW** (SK6812) | `LEDW` (4 B) | `WStrip<N>` | `WStripGroup<M>` | `WStripAllocate` |

Die API ist identisch; RGBW fügt nur den Weißkanal hinzu:

```cpp
WStrip<30> strip{2};
strip.begin();
strip.fill(0, 0, 0, 255);    // pure white through the W channel
strip.fill(255, 0, 0);       // red; the 3-argument fill sets W = 0
strip[10] = {0, 0, 255, 80}; // per-pixel R,G,B,W
strip[11] = hsvW(1000);      // RGBW color helpers -> LEDW (W = 0)
strip.fill(kelvinW(3000));   // no cast, no ambiguity
```

Ein `LEDW` ist ein gepackter 4-Byte-Wert; seine 32-Bit-Form ist `0xRRGGBBWW` und `operator[]` indiziert `0=R, 1=G, 2=B, 3=W`. Nutze die RGBW-Farb-Helfer `hsvW()` / `kelvinW()` — sie liefern ein `LEDW` (mit `W = 0`), sodass RGBW-Code nie den Typ `LED` berührt. Um ein vorhandenes `LED` bewusst hochzustufen, rufe `led.toLEDW()` auf (es gibt keine implizite RGB→RGBW-Konvertierung: `strip[i] = hsv(x)` ist ein absichtlicher Kompilierfehler, der dich zu `hsvW` / `toLEDW` lenkt).

Die Familien RGB und RGBW sind absichtlich unterschiedliche Typen: eine `WStripGroup` akzeptiert nur `WStrip`s, das Mischen von Formaten in einer Gruppe ist also ein Kompilierfehler. Low-Level-Freifunktionen spiegeln die RGB-Varianten — `fill(LEDW*, len, r, g, b, w)`, `fillFromTo(LEDW*, …)` usw. RGBW nutzt dieselbe Signalisierung wie WS2812B — prüfe das genaue Timing an deiner SK6812-Charge.

## Asynchrones Refresh (ESP32 / RP2040)

Auf ESP32/RP2040 nehmen die Streifen-Templates einen optionalen `bool ASYNC` (Standard `true`):

- `Strip<N>` / `WStrip<N>` — **async**: `show()` erstellt einen Schnappschuss der Pixel in einen streifeneigenen Sendepuffer, startet die Übertragung und kehrt zurück; du darfst den Streifen sofort bearbeiten. Rufe `wait()`, bevor du dich auf den Abschluss verlässt, oder `busy()` zum Pollen. Verschiedene Streifen senden nebenläufig. Kostet einen zusätzlichen statischen Puffer (`2 x Pixel` Bytes insgesamt).
- `Strip<N, false>` / `WStrip<N, false>` — **blockierend**: kein Sendepuffer, `show()` kehrt zurück, wenn der Frame auf der Leitung ist. Halber RAM, aber die Übertragung liest den Live-Puffer, sodass das Bearbeiten aus einer anderen Task während `show()` einen eigenen Mutex braucht.

AVR/ESP8266 haben keinen `ASYNC`-Parameter (sie bit-bangen mit deaktivierten Interrupts, `Strip<N,false>` ist dort ein Kompilierfehler) — `Strip<N>` ist einfach blockierend.

## Installation

**Arduino IDE** — kopiere das Repository in deinen Ordner `Arduino/libraries/` (oder nutze *Sketch → Include Library → Add .ZIP Library*), dann `#include <PixelPerfect.h>`.

**PlatformIO** — füge die Bibliothek zu `lib_deps` hinzu oder lege das Repository in den Ordner `lib/` des Projekts. Deklariere die Zielarchitektur(en) über die üblichen `platform`/`board`-Einstellungen; das Bibliotheks-Manifest bewirbt `avr`, `esp32` und `rp2040`.

**ESP-IDF** — siehe **ESP-IDF (ohne Arduino)** unten.

## Schnellstart

```cpp
#include <PixelPerfect.h>

using namespace pix;

Strip<39> strip{2}; // 39 pixels, data on pin 2

void setup() {
    strip.begin();            // configure the pin / acquire the driver
    strip.brightness = 120;   // 0..255, applied on every show()

    strip.fill(0, 255, 255);  // set all pixels to cyan
    strip.show();             // push the buffer to the strip

    delay(2000);

    strip.clear();            // set all pixels to off
    strip[10] = {255, 255, 0}; // address a single pixel
    strip[10].b = 255;        // or a single color channel
    strip.show(255);          // push once at full brightness, leaving `brightness` untouched
}

void loop() {}
```

## Grundkonzepte

- **Farbmodell.** Ein Pixel ist ein `pix::LED` (Alias `pix::Color`), ein gepackter 3-Byte-Wert. Du arbeitest damit als RGB — der Konstruktor nimmt `(r, g, b)`, `operator[]` indiziert `0=R, 1=G, 2=B`, und die 32-Bit-Form ist `0xRRGGBB`. Die Bytes werden intern in der WS2812B-Leitungsreihenfolge (GRB) gespeichert, sodass beim Refresh keine Umsortierung nötig ist; ein Implementierungsdetail, das du ignorieren kannst.
- **Helligkeit.** Jeder Streifen hat ein öffentliches Feld `brightness` (0–255). `show()` skaliert jeden Kanal per schneller Ganzzahl-Näherung, `Kanal * (brightness + 1) / 256`, sodass `255` volle Ausgabe und `0` aus ist. `show(uint8_t)` überschreibt die Helligkeit für ein einzelnes Refresh, ohne den gespeicherten Wert zu ändern.
- **Reverse.** Ein Streifen mit `reverse = true` kehrt die logische Indexreihenfolge um, sodass Index `0` auf das physisch letzte Pixel zeigt. Nützlich, wenn ein Streifen Ende-an-Ende mit einem anderen montiert ist.
- **Speichermodell.** `Strip<N>` hält seine `N` Pixel in einem Member-Array — kein Heap, Größe zur Kompilierzeit bekannt. `StripAllocate` hält sie auf dem Heap und lässt sich zur Laufzeit mit `changeLength()` in der Größe ändern.
- **Zugriff außerhalb des Bereichs ist sicher.** Ein Index über die Länge hinaus liefert eine Referenz auf ein gemeinsames Wegwerf-Pixel, statt Speicher zu beschädigen; Schreibzugriffe darauf haben keine sichtbare Wirkung.

---

# API-Referenz

Alle Symbole leben im Namespace `pix`.

## `pix::LED` (Alias `pix::Color`)

Ein einzelnes Pixel: drei 8-Bit-Kanäle, gepackt auf genau 3 Bytes.

### Konstruktoren

```cpp
LED();                                 // black (0, 0, 0)
LED(uint8_t r, uint8_t g, uint8_t b);  // from channels
LED(uint32_t color);                   // from a packed 0xRRGGBB value
```

| Parameter | Beschreibung |
| :-------- | :----------- |
| `r`, `g`, `b` | Rot-, Grün- und Blaukanal (0–255). |
| `color` | Gepackte Farbe als `0xRRGGBB`. |

Die Argumentanzahl wählt den Konstruktor: **drei** Zahlen werden als Kanäle gelesen (`LED{255, 0, 0}`), eine **einzelne** Zahl als gepackte Farbe (`LED{0xFF0000}` ist Rot). Das heißt, ein einzelner Wert ist immer die gepackte Form — `LED{0x20}` ist `0x000020` (Blau = 32), nicht Rot. Ein- oder zweiargumentige Konstruktion gibt es bewusst nicht, damit eine einzelne Ganzzahl nie zwischen den beiden Konstruktoren mehrdeutig ist.

### Operatoren

```cpp
explicit operator uint32_t() const;        // -> 0xRRGGBB  (explicit: use (uint32_t)pixel)
uint8_t& operator[](uint8_t index);        // 0=R, 1=G, 2=B
LED& operator=(uint32_t color);            // assign from 0xRRGGBB
LED& operator=(const LED& other);
bool operator==(const LED& other) const;
bool operator==(uint32_t color) const;
bool operator!=(const LED& other) const;
bool operator!=(uint32_t color) const;
LEDW toLEDW() const;                        // promote to RGBW, W = 0
```

`operator[]` liefert eine Referenz auf den angeforderten Kanal, lesbar oder schreibbar (`pixel[0] = 255;`). Der Index wird modulo 3 reduziert, `0`/`1`/`2` wählen also R/G/B, größere Werte laufen um, statt zu scheitern. Die `uint32_t`-Konvertierung ist **explizit** — schreibe `uint32_t packed = (uint32_t)pixel;` — sodass ein `LED` nie stillschweigend zur Ganzzahl wird (was früher mit den RGBW-Overloads kollidierte). Das `LEDW`-Pixel spiegelt diese API mit einem vierten `W`-Kanal und der gepackten Form `0xRRGGBBWW`.

## Streifen

Alle Streifentypen leiten vom abstrakten `pix::StripInterface` ab und teilen daher dieselbe API. Zwei konkrete Implementierungen werden bereitgestellt.

### Typen

```cpp
// AVR / ESP8266:
template <uint16_t LEN>
using Strip = StripTemplate<LEN>;

// ESP32 / RP2040: an extra opt-out ASYNC parameter (see "Asynchronous refresh"):
template <uint16_t LEN, bool ASYNC = true>
using Strip = StripTemplate<LEN, ASYNC>;

class StripAllocate;                       // pixels stored on the heap, resizable
using StripHandler = StripInterface*;      // pointer to any strip, used by StripGroup
```

`Strip<LEN>` baut überall. `Strip<LEN, false>` wird nur auf ESP32/RP2040 akzeptiert — der `ASYNC`-Parameter ist auf AVR/ESP8266 bewusst nicht verfügbar (siehe **Asynchrones Refresh**).

### Konstruktoren

```cpp
Strip<LEN>(uint8_t pin = 255, bool reverse = false);
Strip<LEN, false>(uint8_t pin = 255, bool reverse = false);   // ESP32/RP2040 only
StripAllocate(uint16_t pixelCount, uint8_t pin = 255, bool reverse = false);
StripAllocate();                           // empty; set length later with changeLength()
```

| Parameter | Beschreibung |
| :-------- | :----------- |
| `LEN` / `pixelCount` | Anzahl der Pixel im Streifen. |
| `ASYNC` | *(nur ESP32/RP2040, Standard `true`.)* `true` = doppelt gepuffert, nicht blockierendes `show()`; `false` = einfach gepuffert, blockierendes `show()` (halber RAM). |
| `pin` | Datenpin. `255` bedeutet "nicht zugewiesen" — setze ihn über den Konstruktor oder `setPin()` vor `begin()`. |
| `reverse` | Kehre die logische Pixelreihenfolge um. |

### Öffentliche Felder

| Feld | Typ | Beschreibung |
| :--- | :-- | :----------- |
| `brightness` | `uint8_t` | Von `show()` angewandte Helligkeit; Standard `120`. |
| `reverse` | `bool` | Ob die logische Indizierung umgekehrt ist. |

### Lebenszyklus

```cpp
void begin();
```
Bereitet den Streifen für die Ausgabe vor: konfiguriert den Datenpin und, auf ESP32, belegt und konfiguriert den an diesen Pin gebundenen RMT-Kanal. Einmal in `setup()` / vor dem ersten `show()` aufrufen.

### Pixel adressieren

```cpp
LED& led(uint16_t i);
const LED& led(uint16_t i) const;
LED& operator[](uint16_t i);
const LED& operator[](uint16_t i) const;
```

Liefern eine Referenz auf Pixel `i` (unter Beachtung von `reverse`). `led(i)` und `operator[](i)` sind gleichwertig. Ein Index `>= length()` liefert ein gemeinsames Dummy-Pixel, sodass Zugriff außerhalb des Bereichs weder abstürzt noch den Streifen beeinflusst.

### Farben setzen

```cpp
void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
void fillFromTo(uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(const Color& color, uint16_t from, uint16_t to);
```

`fill(...)` setzt jedes Pixel auf eine Farbe. `fillFromTo(...)` setzt den geschlossenen Bereich `[from, to]`. Die Bereichsaufrufe tun nichts, wenn `from > to` oder `to >= length()`.

### Aktualisieren

```cpp
void show();                // scale by `brightness`, then transmit
void show(uint8_t bright);  // scale by `bright` for this refresh only

// ESP32 / RP2040 only:
void wait();                // block until this strip's async frame has finished
bool busy();                // true while an async frame is still on the wire
```

Überträgt den Pixelpuffer an den physischen Streifen. `show()` nutzt das Feld `brightness` des Streifens; `show(bright)` nutzt den angegebenen Wert, ohne das Feld zu ändern.

Auf einem standardmäßigen (asynchronen) `Strip<LEN>` auf ESP32/RP2040 kehrt `show()` zurück, sobald die Übertragung gestartet ist; nutze `wait()`/`busy()` zur Synchronisation. Auf AVR/ESP8266 und auf `Strip<LEN, false>` blockiert `show()`, bis der Frame vollständig gesendet ist. Siehe **Asynchrones Refresh**.

### Sonstiges

```cpp
uint16_t length() const;    // pixel count
void clear();               // set every pixel to off (does not transmit)
void setPin(uint8_t p);
uint8_t getPin() const;
bool ok() const;             // ESP32 / RP2040: false, wenn dieser Pin keinen Kanal hat
void changeLength(uint16_t l);   // StripAllocate only: reallocate to l pixels
```

`clear()` aktualisiert nur den Puffer; rufe danach `show()`, um den physischen Streifen zu löschen. `ok()` meldet, ob der Pin dieses Streifens gerade einen Hardware-Kanal hat (oder noch bekommen kann); auf AVR/ESP8266, die kein Kanalkonzept kennen, ist es immer `true`. Siehe **Kanal-Pooling und -Warteschlange**.

## Streifengruppen

`pix::StripGroup<N>` präsentiert mehrere unabhängige Streifen als einen zusammenhängenden, nullbasierten Indexraum, sodass eine Animation über mehrere Datenpins reichen kann, ohne Grenzen von Hand zu verfolgen. Jeder Mitglieds-Streifen hält seinen eigenen Pixelpuffer und wird an seinem eigenen Pin aktualisiert.

```cpp
template <uint8_t STRIP_COUNT>
class StripGroup;                          // derives from StripGroupInterface

using StripGroupHandler = StripGroupInterface*;
```

### Konstruktoren

```cpp
StripGroup();                                       // empty
StripGroup(const StripHandler (&strips)[STRIP_COUNT]); // from an array of strip pointers
```

Fülle eine Gruppe, indem du ein Array von `StripHandler` (Zeiger auf bestehende Streifen) übergibst. Die Streifen werden referenziert, nicht kopiert, sie müssen die Gruppe also überleben.

```cpp
Strip<30> a{2};
Strip<30> b{4};
StripHandler handlers[2] = { &a, &b };
StripGroup<2> group{handlers};
```

### Öffentliche Felder

| Feld | Typ | Beschreibung |
| :--- | :-- | :----------- |
| `brightness` | `uint8_t` | An `show()` jedes Streifens übergebene Helligkeit; Standard `128`. |

### Methoden

```cpp
uint32_t length() const;        // cached total pixel count across all strips
uint32_t recalcLength();        // recompute and cache the total; call after populating the group
void begin();                   // call begin() on every member strip
void clear();                   // clear every member strip's buffer
void show();                    // refresh every strip using the group `brightness`
void show(uint8_t bright);      // refresh every strip using `bright`
void showStrip(uint8_t strip);  // refresh only the strip at the given slot

void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to);
void fillFromTo(uint32_t color, uint32_t from, uint32_t to);
void fillFromTo(const Color& color, uint32_t from, uint32_t to);

LED& led(uint32_t i);           // pixel by global index (spans all strips)
const LED& led(uint32_t i) const;
LED& operator[](uint32_t i);    // equivalent to led(i)
const LED& operator[](uint32_t i) const;

StripHandler strip(uint8_t s);  // pointer to the member strip in slot s (nullptr if out of range)
```

Rufe `recalcLength()` einmal nach dem Aufbau der Gruppe auf (und erneut, wenn du Streifen tauschst), damit `length()`, `led()` und die Bereichs-Fills die Gesamtgröße kennen. Ein Index über die Gesamtgröße hinaus liefert das gemeinsame Dummy-Pixel.

## Freifunktionen (Low-Level)

Diese arbeiten direkt auf einem rohen `LED*`-Puffer und sind das, was die Streifenklassen intern aufrufen. Nutze sie, wenn du den Puffer selbst verwaltest.

```cpp
void begin(uint8_t pin);
void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright = 255, bool scale = true);
void clear(LED* leds, uint16_t len);

void fill(LED* leds, uint16_t len, uint32_t color);
void fill(LED* leds, uint16_t len, const Color& color);
void fill(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b);
void fillFromTo(LED* leds, uint16_t len, uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, const Color& color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
```

| Parameter | Beschreibung |
| :-------- | :----------- |
| `leds` | Zeiger auf den Pixelpuffer. |
| `len` | Anzahl der Pixel im Puffer. |
| `pin` | Datenpin. |
| `bright` | Helligkeit (0–255), angewandt wenn `scale` `true` ist. |
| `scale` | Wenn `false`, überträgt `show()` den Puffer wortgetreu ohne Helligkeitsskalierung. |
| `from`, `to` | Geschlossener Pixelbereich für `fillFromTo`. |

`begin(pin)` konfiguriert den Pin (und belegt auf ESP32 den RMT-Kanal). `show(...)` führt eine Übertragung durch und erzwingt die WS2812B-Reset-Lücke zwischen Frames.

### Farb-Helfer

```cpp
Color  hsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);
Color  kelvin(uint16_t temp);
ColorW hsvW(uint16_t hue, uint8_t sat = 255, uint8_t val = 255); // RGBW: LEDW, W = 0
ColorW kelvinW(uint16_t temp);                                   // RGBW: LEDW, W = 0
Color& gamma32(Color& color);   // gamma-correct in place, returns the same reference
uint32_t gamma32(uint32_t color); // gamma-correct a packed color
```

| Funktion | Beschreibung |
| :------- | :----------- |
| `hsv` | Wandelt HSV in RGB. `hue` deckt über seinen ganzen Bereich `0–65535` den kompletten Farbkreis ab; `sat` und `val` sind `0–255`. |
| `kelvin` | Nähert die RGB-Farbe eines schwarzen Körpers bei `temp` Kelvin (Tabelle deckt etwa 520–10000 K). |
| `hsvW` / `kelvinW` | Wie `hsv` / `kelvin`, liefern aber ein `LEDW` (`W = 0`), zur Nutzung mit `WStrip` ohne jede Umwandlung. |
| `gamma32` | Wendet perzeptuelle Gammakorrektur an — auf ein `Color` an Ort und Stelle oder auf einen `0xRRGGBB`-Wert. |

---

## Asynchrones Refresh (ESP32 / RP2040)

Auf ESP32 und RP2040 wird die Übertragung von Hardware getrieben (RMT / PIO+DMA), kann also laufen, während die CPU etwas anderes tut. Das wird über den Template-Parameter `ASYNC` verfügbar gemacht, pro Streifen wählbar:

| | `Strip<LEN>` (`ASYNC = true`, Standard) | `Strip<LEN, false>` |
| :-- | :-- | :-- |
| Puffer | Bearbeitungspuffer **+** Sendepuffer (Schnappschuss) | nur Bearbeitungspuffer |
| RAM pro Streifen | `6 × LEN` Bytes | `3 × LEN` Bytes |
| `show()` | nicht blockierend: Schnappschuss+Skalierung in den Sendepuffer, startet die Übertragung, kehrt zurück | blockierend: kehrt zurück, wenn der Frame auf der Leitung ist |
| Mehrere Streifen | senden **nebenläufig** (jeder auf eigenem Kanal) | einer nach dem anderen |
| Pixel direkt nach `show()` bearbeiten | immer sicher (Übertragung liest den Schnappschuss) | sicher aus einer Task; nebenläufige Tasks brauchen einen Mutex |

```cpp
Strip<60> a{2};     // async
Strip<60> b{4};     // async

a.fill(...); b.fill(...);
a.show();           // starts A, returns immediately
b.show();           // starts B while A is still on the wire -> both transmit at once
a.wait(); b.wait(); // block until each frame is done
```

Weil `show()` in den Sendepuffer schnappschusst, darfst du die Pixel des Streifens direkt nach der Rückkehr weiter bearbeiten, ohne den laufenden Frame zu zerreißen. `wait()` vor einem zweiten `show()` auf *demselben* Streifen ist implizit (ein Streifen beendet seinen vorherigen Frame, bevor der nächste startet); verschiedene Streifen überlappen frei. `busy()` pollt ohne zu blockieren. Siehe `examples/async_multistrip`.

`ASYNC` gibt es nur auf ESP32/RP2040. Auf AVR/ESP8266 existiert der Parameter nicht (`Strip<LEN, false>` ist dort ein Kompilierfehler): diese bit-bangen mit deaktivierten Interrupts, `show()` ist also immer blockierend und der Frame kann nicht zerrissen werden.

### Kanal-Pooling und -Warteschlange (ESP32 / RP2040)

Jeder aktive Pin braucht einen Hardware-Kanal (ein RMT-Kanal auf ESP32; eine PIO-Zustandsmaschine + ein DMA-Kanal auf RP2040), und jeder Chip hat davon nur eine Handvoll: 8 auf einem klassischen ESP32, 4 auf S2/S3, 2 TX auf C3 (siehe `SOC_RMT_CHANNELS_PER_GROUP` für deinen Chip); 8 auf RP2040 (2 PIO-Blöcke × 4 Zustandsmaschinen). Mehr Streifen zu deklarieren als das ist kein Problem — was passiert, wenn du sie tatsächlich benutzt, hängt vom Treiber ab:

- **RP2040 und ESP32 auf dem alten `driver/rmt.h`-Kern (Arduino-Kern 2.x / IDF 4):** Kanäle werden gepoolt und in eine Warteschlange gestellt. Ein `show()`/asynchroner `show()`-Aufruf bekommt den Kanal, den er schon besitzt, oder beansprucht einen freien, oder — sobald der Pool voll ist — stiehlt den am längsten ungenutzten **untätigen** Kanal und bindet ihn an den neuen Pin um. Das Umbinden ist billig (nur der Ausgabe-GPIO ändert sich; kein Kanal/DMA wird abgebaut) und rührt nie einen Kanal an, der noch mitten in der Übertragung ist. Sind gerade alle Kanäle beschäftigt, wartet der Aufruf einfach (gibt anderen Tasks die Zeit), bis einer frei wird — eine kurze, einmalige Verzögerung, kein Fehler.
- **ESP32 auf dem neuen `driver/rmt_tx.h`-Kern (Arduino-Kern 3.x / IDF 5+):** kein Pooling. Dieser Treiber hat keine öffentliche API, um einen bestehenden Kanal ohne Löschen und Neuerstellen (samt Encoder) an einen anderen GPIO umzubinden — zu teuer, um es transparent bei jedem Frame zu tun — sodass, sobald alle `SOC_RMT_CHANNELS_PER_GROUP`-Kanäle anderen Pins gehören, weitere Pins schlicht keinen bekommen können. Das ist eine Einschränkung von Espressifs neuerem Treiber, nicht dieser Bibliothek: sogar FastLED umgeht das, indem es an der öffentlichen RMT-API vorbei direkt auf die GPIO-Matrix zugreift — ein Wartungsaufwand, den diese Bibliothek bewusst vermeidet.

Rufe `strip.ok()` auf, um zu prüfen, ob ein Pin gerade einen Kanal hat (oder bekommen kann). Auf IDF5 ist ein `false` dauerhaft (und wird zusätzlich einmal pro Pin per `ESP_LOGE` geloggt), statt dass der Streifen stillschweigend nie leuchtet; auf RP2040/IDF4 bedeutet ein kurzzeitiges `false` nur, dass gerade alle Kanäle beschäftigt sind — der nächste `show()`-Aufruf gelingt trotzdem, nur in die Warteschlange gestellt.

## Plattform-Hinweise

### ESP32 und FreeRTOS

Der RMT-Treiber deaktiviert während der Übertragung nie Interrupts oder den Scheduler, sodass andere Tasks — und der zweite Kern — normal weiterlaufen. `examples/esp32_freertos_concurrency` zeigt das: eine zweite Task hält einen gleichmäßigen Takt, während ein Streifen in einer engen Schleife aktualisiert wird.

Beachte:

- **Keine interne Sperrung.** Ein Streifen oder eine Gruppe hat keine eingebaute Synchronisation. Mit einem asynchronen `Strip<LEN>` kannst du Pixel direkt nach `show()` frei bearbeiten, aber zwei Tasks am *selben* Streifen müssen dennoch serialisiert werden. Bei `Strip<LEN, false>` (und auf AVR/ESP8266) kann ein nebenläufiger Schreibzugriff während `show()` den Frame zerreißen — sichere ihn mit einem Mutex:

  ```cpp
  SemaphoreHandle_t stripLock = xSemaphoreCreateMutex();
  // ...
  xSemaphoreTake(stripLock, portMAX_DELAY);
  strip.fill(255, 0, 0);
  strip.show();
  xSemaphoreGive(stripLock);
  ```

- **Kanalbudget.** Ein klassischer ESP32 hat 8 RMT-Kanäle (weniger auf S2/S3/C3 — siehe `SOC_RMT_CHANNELS_PER_GROUP` für deinen Chip). Auf dem alten Kern (IDF 4) teilen sich mehr Pins als das den Pool transparent per Warteschlange; auf dem neuen Kern (IDF 5+) ist das eine harte Grenze. Siehe **Kanal-Pooling und -Warteschlange**.

Beide arduino-esp32-Kerngenerationen werden unterstützt und automatisch gewählt: die alte `driver/rmt.h`-API (Kern 2.x / IDF 4) und die neuere Encoder-API `driver/rmt_tx.h` (Kern 3.x / IDF 5). Die Bibliothek selbst allokiert nichts dynamisch (Encoder/Registry sind statisch); das RMT-SDK allokiert seine eigenen Kanalstrukturen einmal pro Pin beim Setup, was der Treibernutzung inhärent ist.

### RP2040 / Raspberry Pi Pico

Zielt auf den arduino-pico-Kern (Earle Philhower); eine PIO-Zustandsmaschine erzeugt die WS2812B-Wellenform in Hardware, vom PIO getaktet, sodass keine Interrupt-Sperre nötig ist. Zwei Übertragungspfade, beide ohne Allokation (überhaupt kein Heap):

- **Async `Strip<LEN>`:** ein einzelner DMA-Kanal streamt den vorskalierten Sendepuffer in das PIO-FIFO und kehrt zurück; der Abschluss wird in `wait()`/`busy()` gepollt. Bis zu 8 gleichzeitig aktive Streifen (2 PIO-Blöcke × 4 Zustandsmaschinen); darüber hinaus werden Kanäle gepoolt und in eine Warteschlange gestellt — siehe **Kanal-Pooling und -Warteschlange**.
- **Blockierend (`Strip<LEN, false>` und das Low-Level-`pix::show()`):** die CPU skaliert jedes Byte und füttert das FIFO direkt — kein Puffer, kein DMA. Das 8 Einträge tiefe TX-FIFO (~80 µs) absorbiert Scheduling-Jitter.

### ESP8266

Zielt auf den ESP8266-Arduino-Kern. Die Wellenform wird bit-gebangt und gegen den Xtensa-Zykluszähler (CCOUNT) getaktet, abgeleitet aus der aktuellen CPU-Frequenz, sodass sie bei 80 und 160 MHz korrekt ist. Jeder GPIO funktioniert (auch Pin 16), anders als die I2S/UART-Tricks, die auf einen festen Pin festlegen. Die Helligkeit wird inline pro Byte skaliert, sodass beim Refresh nichts allokiert wird.

Interrupts sind für die Dauer eines Frames deaktiviert (derselbe Kompromiss wie beim AVR-Treiber), weil ein falsch getakteter Interrupt das Signal verfälschen würde. Der Kern ist einthreadig (kein FreeRTOS), das blockiert also keinen Scheduler, aber halte die Streifen sinnvoll kurz: Interrupts bleiben etwa `Pixel × 30 µs` aus, und sie sehr lange (Tausende Pixel) auszuschalten kann die WLAN-Bedienung stören.

### ESP-IDF (ohne Arduino)

Die Bibliothek baut auch als natives **ESP-IDF-Komponent**, ganz ohne Arduino-Schicht. Sie nutzt exakt denselben RMT-Treiber und dieselbe `pix::`-API; die wenigen Arduino-Symbole, auf die die Quellen angewiesen sind (`micros`, `pinMode`/`digitalWrite`, `PROGMEM`/`pgm_read_byte`), liefert `pix_platform.h`, das je nach Definition von `ARDUINO` und `ESP_PLATFORM` zwischen `<Arduino.h>` und einem reinen IDF-Shim umschaltet.

Mache das Repository deinem Projekt als Komponent sichtbar — entweder unter `components/PixelPerfect/` ablegen oder `EXTRA_COMPONENT_DIRS` darauf zeigen lassen — dann fordere es aus deinem App-Komponent an:

```cmake
idf_component_register(SRCS "app_main.cpp" INCLUDE_DIRS "." REQUIRES PixelPerfect)
```

Die Nutzung ist identisch zur Arduino-API; nur der Einstiegspunkt unterscheidet sich (`app_main()` statt `setup()`/`loop()`):

```cpp
#include "PixelPerfect.h"
using namespace pix;

static Strip<39> strip{2};

extern "C" void app_main(void) {
    strip.begin();
    strip.brightness = 120;
    strip.fill(0, 255, 255);
    strip.show();
}
```

`examples/idf_basic` ist ein vollständiges, baubares Projekt (`idf.py build`).

> **Hinweis:** Ab ESP-IDF ≥ 5.3 wurden die RMT- und GPIO-Treiber in eigene Komponenten aufgeteilt. Meldet ein Build ein fehlendes `driver/rmt_tx.h` oder `driver/gpio.h`, füge `esp_driver_rmt esp_driver_gpio` zu `REQUIRES` des Komponents in der Wurzel-`CMakeLists.txt` hinzu.

## Konfiguration zur Kompilierzeit

| Makro | Wirkung |
| :---- | :------ |
| `DISABLE_GAMMA_LUT` | Berechnet Gammakorrektur mit Fließkomma-Mathematik statt der Flash-Lookup-Tabelle (kleinerer Flash-Fußabdruck, langsamer). |
| `DISABLE_KELVIN_LUT` | Berechnet `kelvin()` mit Fließkomma-Mathematik statt der Flash-Lookup-Tabelle. |

## Beispiele

| Sketch | Plattform | Zeigt |
| :----- | :-------- | :---- |
| `examples/esp32_basic` | ESP32 (Arduino) | Minimale fill / show-Nutzung. |
| `examples/esp32_freertos_concurrency` | ESP32 (Arduino) | Eine Refresh-Schleife neben einer anderen FreeRTOS-Task, ohne sie zu stören. |
| `examples/esp8266_basic` | ESP8266 (Arduino) | Minimale fill / show-Nutzung. |
| `examples/rp2040_basic` | RP2040 (Arduino) | Minimale fill / show-Nutzung. |
| `examples/rgbw_basic` | beliebig (Arduino) | RGBW (`WStrip` / `LEDW`, SK6812) mit dem Weißkanal. |
| `examples/async_multistrip` | ESP32 / RP2040 | Nicht blockierendes `show()`, zwei Streifen senden gleichzeitig, `wait()` und ein `Strip<N, false>`-Vergleich. |
| `examples/idf_basic` | ESP32 (ESP-IDF) | Dieselbe API aus einem reinen `app_main()`-Projekt. |

## Lizenz

Siehe [LICENSE](LICENSE).

</details>

<details>
<summary><b>🇫🇷 Français</b></summary>

<br>

Une bibliothèque C++ légère pour piloter les rubans de LED adressables **WS2812B** (« NeoPixel »). Elle se veut plus simple que FastLED tout en évitant l'allocation dynamique de mémoire sur le chemin critique : le type de ruban par défaut garde son tampon de pixels dans un tableau de taille fixe, dimensionné statiquement.

Le même code source se compile sans changement sur plusieurs cibles, chacune utilisant le meilleur mécanisme de sortie offert par le matériel :

| Plateforme | Framework | Mécanisme de sortie | Source |
| :--------- | :-------- | :------------------ | :----- |
| AVR (p. ex. ATmega2560) | Arduino | Bit-banging compté en cycles (assembleur en ligne) | `atmega_show.cpp` |
| ESP32 (toutes variantes) | Arduino **ou** ESP-IDF | Périphérique matériel RMT | `esp32_show.cpp` |
| ESP8266 | Arduino | Bit-banging cadencé par CCOUNT | `esp8266_show.cpp` |
| RP2040 / Raspberry Pi Pico | Arduino (arduino-pico) | Machine à états PIO + DMA | `rp2040_show.cpp` |

Le bon backend est choisi automatiquement à la compilation d'après des macros de framework prédéfinies — tu ne le choisis jamais à la main.

## Auteur

- [@syzyf97](https://www.github.com/syzyf97)

## Fonctionnalités

- Ruban de taille fixe sans allocation (`Strip<N>`) plus une variante optionnelle sur le tas (`StripAllocate`).
- **RGB et RGBW** (p. ex. SK6812) en deux familles distinctes et sûres en type : `Strip`/`LED` et `WStrip`/`LEDW`.
- **Rafraîchissement non bloquant** sur ESP32/RP2040 (`Strip<N>` est asynchrone par défaut), donc plusieurs rubans peuvent émettre en même temps ; `wait()`/`busy()` pour synchroniser.
- Mise à l'échelle de la luminosité par ruban et par appel.
- Aides de couleur pratiques : HSV, température de couleur (Kelvin) et correction gamma, par défaut basées sur des tables de correspondance en flash.
- Regroupement de plusieurs rubans dans un seul espace d'index logique (`StripGroup` / `WStripGroup`).
- Compatible FreeRTOS sur ESP32 et RP2040 : un rafraîchissement ne bloque jamais l'ordonnanceur ni le second cœur.

## Formats de couleur : RGB et RGBW

Deux familles parallèles et sûres en type partagent une seule implémentation :

| | Pixel | Ruban | Groupe | Variante sur tas |
| :-- | :-- | :-- | :-- | :-- |
| **RGB** (WS2812B) | `LED` (3 o) | `Strip<N>` | `StripGroup<M>` | `StripAllocate` |
| **RGBW** (SK6812) | `LEDW` (4 o) | `WStrip<N>` | `WStripGroup<M>` | `WStripAllocate` |

L'API est identique ; RGBW ajoute simplement le canal blanc :

```cpp
WStrip<30> strip{2};
strip.begin();
strip.fill(0, 0, 0, 255);    // pure white through the W channel
strip.fill(255, 0, 0);       // red; the 3-argument fill sets W = 0
strip[10] = {0, 0, 255, 80}; // per-pixel R,G,B,W
strip[11] = hsvW(1000);      // RGBW color helpers -> LEDW (W = 0)
strip.fill(kelvinW(3000));   // no cast, no ambiguity
```

Un `LEDW` est une valeur empaquetée de 4 octets ; sa forme 32 bits est `0xRRGGBBWW` et `operator[]` indexe `0=R, 1=G, 2=B, 3=W`. Utilise les aides de couleur RGBW `hsvW()` / `kelvinW()` — elles renvoient un `LEDW` (avec `W = 0`), donc le code RGBW ne touche jamais au type `LED`. Pour promouvoir volontairement un `LED` existant, appelle `led.toLEDW()` (il n'y a pas de conversion implicite RGB→RGBW : `strip[i] = hsv(x)` est une erreur de compilation volontaire qui t'oriente vers `hsvW` / `toLEDW`).

Les familles RGB et RGBW sont des types distincts à dessein : un `WStripGroup` n'accepte que des `WStrip`, donc mélanger les formats dans un groupe est une erreur de compilation. Les fonctions libres bas niveau reflètent celles de RGB — `fill(LEDW*, len, r, g, b, w)`, `fillFromTo(LEDW*, …)`, etc. RGBW utilise la même signalisation que WS2812B — vérifie le timing exact sur ton lot de SK6812.

## Rafraîchissement asynchrone (ESP32 / RP2040)

Sur ESP32/RP2040, les templates de ruban prennent un `bool ASYNC` optionnel (par défaut `true`) :

- `Strip<N>` / `WStrip<N>` — **async** : `show()` prend un instantané des pixels dans un tampon d'envoi propre au ruban, démarre le transfert et retourne ; tu peux modifier le ruban immédiatement. Appelle `wait()` avant de compter sur l'achèvement, ou `busy()` pour interroger. Des rubans différents émettent en parallèle. Coûte un tampon statique supplémentaire (`2 x pixels` octets au total).
- `Strip<N, false>` / `WStrip<N, false>` — **bloquant** : pas de tampon d'envoi, `show()` retourne quand la trame est sur le fil. Moitié moins de RAM, mais le transfert lit le tampon vivant, donc le modifier depuis une autre tâche pendant `show()` demande ton propre mutex.

AVR/ESP8266 n'ont pas de paramètre `ASYNC` (ils font du bit-banging avec les interruptions désactivées, donc `Strip<N,false>` y est une erreur de compilation) — `Strip<N>` est simplement bloquant.

## Installation

**Arduino IDE** — copie le dépôt dans ton dossier `Arduino/libraries/` (ou utilise *Sketch → Include Library → Add .ZIP Library*), puis `#include <PixelPerfect.h>`.

**PlatformIO** — ajoute la bibliothèque à `lib_deps`, ou dépose le dépôt dans le dossier `lib/` du projet. Déclare la ou les architectures cibles via les réglages `platform`/`board` habituels ; le manifeste de la bibliothèque annonce `avr`, `esp32` et `rp2040`.

**ESP-IDF** — voir **ESP-IDF (sans Arduino)** ci-dessous.

## Démarrage rapide

```cpp
#include <PixelPerfect.h>

using namespace pix;

Strip<39> strip{2}; // 39 pixels, data on pin 2

void setup() {
    strip.begin();            // configure the pin / acquire the driver
    strip.brightness = 120;   // 0..255, applied on every show()

    strip.fill(0, 255, 255);  // set all pixels to cyan
    strip.show();             // push the buffer to the strip

    delay(2000);

    strip.clear();            // set all pixels to off
    strip[10] = {255, 255, 0}; // address a single pixel
    strip[10].b = 255;        // or a single color channel
    strip.show(255);          // push once at full brightness, leaving `brightness` untouched
}

void loop() {}
```

## Concepts de base

- **Modèle de couleur.** Un pixel est un `pix::LED` (alias `pix::Color`), une valeur empaquetée de 3 octets. Tu le manipules en RGB — le constructeur prend `(r, g, b)`, `operator[]` indexe `0=R, 1=G, 2=B`, et la forme 32 bits est `0xRRGGBB`. Les octets sont stockés en interne dans l'ordre du fil WS2812B (GRB), donc aucune réorganisation n'a lieu au rafraîchissement ; un détail d'implémentation que tu peux ignorer.
- **Luminosité.** Chaque ruban a un champ public `brightness` (0–255). `show()` met à l'échelle chaque canal via une approximation entière rapide, `canal * (brightness + 1) / 256`, donc `255` est la sortie pleine et `0` est éteint. `show(uint8_t)` permet de surcharger la luminosité pour un seul rafraîchissement sans changer la valeur stockée.
- **Reverse.** Construire un ruban avec `reverse = true` inverse l'ordre d'index logique, donc l'index `0` correspond au dernier pixel physique. Utile quand un ruban est monté bout à bout avec un autre.
- **Modèle mémoire.** `Strip<N>` garde ses `N` pixels dans un tableau membre — pas de tas, taille connue à la compilation. `StripAllocate` les garde sur le tas et peut être redimensionné à l'exécution avec `changeLength()`.
- **L'accès hors limites est sûr.** Indexer un ruban au-delà de sa longueur renvoie une référence à un pixel jetable partagé au lieu de corrompre la mémoire ; les écritures dessus n'ont aucun effet visible.

---

# Référence de l'API

Tous les symboles vivent dans l'espace de noms `pix`.

## `pix::LED` (alias `pix::Color`)

Un seul pixel : trois canaux 8 bits, empaquetés en exactement 3 octets.

### Constructeurs

```cpp
LED();                                 // black (0, 0, 0)
LED(uint8_t r, uint8_t g, uint8_t b);  // from channels
LED(uint32_t color);                   // from a packed 0xRRGGBB value
```

| Paramètre | Description |
| :-------- | :---------- |
| `r`, `g`, `b` | Canaux rouge, vert et bleu (0–255). |
| `color` | Couleur empaquetée sous forme `0xRRGGBB`. |

Le nombre d'arguments choisit le constructeur : **trois** nombres sont lus comme des canaux (`LED{255, 0, 0}`), un **seul** nombre comme une couleur empaquetée (`LED{0xFF0000}` est rouge). Cela signifie qu'une valeur seule est toujours la forme empaquetée — `LED{0x20}` vaut `0x000020` (bleu = 32), pas rouge. La construction à un ou deux arguments n'est volontairement pas fournie, afin qu'un entier seul ne soit jamais ambigu entre les deux constructeurs.

### Opérateurs

```cpp
explicit operator uint32_t() const;        // -> 0xRRGGBB  (explicit: use (uint32_t)pixel)
uint8_t& operator[](uint8_t index);        // 0=R, 1=G, 2=B
LED& operator=(uint32_t color);            // assign from 0xRRGGBB
LED& operator=(const LED& other);
bool operator==(const LED& other) const;
bool operator==(uint32_t color) const;
bool operator!=(const LED& other) const;
bool operator!=(uint32_t color) const;
LEDW toLEDW() const;                        // promote to RGBW, W = 0
```

`operator[]` renvoie une référence au canal demandé, on peut donc le lire ou l'écrire (`pixel[0] = 255;`). L'index est réduit modulo 3, donc `0`/`1`/`2` sélectionnent R/G/B et toute valeur plus grande boucle au lieu d'échouer. La conversion en `uint32_t` est **explicite** — écris `uint32_t packed = (uint32_t)pixel;` — pour qu'un `LED` ne se transforme jamais silencieusement en entier (ce qui entrait en conflit avec les surcharges RGBW). Le pixel `LEDW` reflète cette API avec un quatrième canal `W` et une forme empaquetée `0xRRGGBBWW`.

## Rubans

Tous les types de rubans dérivent de l'interface abstraite `pix::StripInterface` et partagent donc la même API. Deux implémentations concrètes sont fournies.

### Types

```cpp
// AVR / ESP8266:
template <uint16_t LEN>
using Strip = StripTemplate<LEN>;

// ESP32 / RP2040: an extra opt-out ASYNC parameter (see "Asynchronous refresh"):
template <uint16_t LEN, bool ASYNC = true>
using Strip = StripTemplate<LEN, ASYNC>;

class StripAllocate;                       // pixels stored on the heap, resizable
using StripHandler = StripInterface*;      // pointer to any strip, used by StripGroup
```

`Strip<LEN>` se compile partout. `Strip<LEN, false>` n'est accepté que sur ESP32/RP2040 — le paramètre `ASYNC` n'est volontairement pas exposé sur AVR/ESP8266 (voir **Rafraîchissement asynchrone**).

### Constructeurs

```cpp
Strip<LEN>(uint8_t pin = 255, bool reverse = false);
Strip<LEN, false>(uint8_t pin = 255, bool reverse = false);   // ESP32/RP2040 only
StripAllocate(uint16_t pixelCount, uint8_t pin = 255, bool reverse = false);
StripAllocate();                           // empty; set length later with changeLength()
```

| Paramètre | Description |
| :-------- | :---------- |
| `LEN` / `pixelCount` | Nombre de pixels du ruban. |
| `ASYNC` | *(ESP32/RP2040 uniquement, par défaut `true`.)* `true` = double tampon, `show()` non bloquant ; `false` = tampon unique, `show()` bloquant (moitié de la RAM). |
| `pin` | Broche de données. `255` signifie « non assignée » — définis-la via le constructeur ou `setPin()` avant `begin()`. |
| `reverse` | Inverse l'ordre logique des pixels. |

### Champs publics

| Champ | Type | Description |
| :---- | :--- | :---------- |
| `brightness` | `uint8_t` | Luminosité appliquée par `show()` ; par défaut `120`. |
| `reverse` | `bool` | Si l'indexation logique est inversée. |

### Cycle de vie

```cpp
void begin();
```
Prépare le ruban pour la sortie : configure la broche de données et, sur ESP32, acquiert et configure le canal RMT lié à cette broche. À appeler une fois dans `setup()` / avant le premier `show()`.

### Adressage des pixels

```cpp
LED& led(uint16_t i);
const LED& led(uint16_t i) const;
LED& operator[](uint16_t i);
const LED& operator[](uint16_t i) const;
```

Renvoient une référence au pixel `i` (en respectant `reverse`). `led(i)` et `operator[](i)` sont équivalents. Un index `>= length()` renvoie un pixel factice partagé, donc l'accès hors limites ne plante pas et n'affecte pas le ruban.

### Définir les couleurs

```cpp
void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
void fillFromTo(uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(const Color& color, uint16_t from, uint16_t to);
```

`fill(...)` met chaque pixel à une seule couleur. `fillFromTo(...)` remplit l'intervalle fermé `[from, to]`. Les appels d'intervalle ne font rien si `from > to` ou `to >= length()`.

### Rafraîchissement

```cpp
void show();                // scale by `brightness`, then transmit
void show(uint8_t bright);  // scale by `bright` for this refresh only

// ESP32 / RP2040 only:
void wait();                // block until this strip's async frame has finished
bool busy();                // true while an async frame is still on the wire
```

Transmet le tampon de pixels au ruban physique. `show()` utilise le champ `brightness` du ruban ; `show(bright)` utilise la valeur fournie sans modifier le champ.

Sur un `Strip<LEN>` par défaut (asynchrone) sur ESP32/RP2040, `show()` retourne dès que le transfert a démarré ; utilise `wait()`/`busy()` pour synchroniser. Sur AVR/ESP8266, et sur `Strip<LEN, false>`, `show()` bloque jusqu'à ce que la trame soit entièrement envoyée. Voir **Rafraîchissement asynchrone**.

### Divers

```cpp
uint16_t length() const;    // pixel count
void clear();               // set every pixel to off (does not transmit)
void setPin(uint8_t p);
uint8_t getPin() const;
bool ok() const;             // ESP32 / RP2040 : false si cette broche n'a pas de canal
void changeLength(uint16_t l);   // StripAllocate only: reallocate to l pixels
```

`clear()` ne met à jour que le tampon ; appelle `show()` ensuite pour éteindre le ruban physique. `ok()` indique si la broche de ce ruban a actuellement (ou peut encore obtenir) un canal matériel ; sur AVR/ESP8266, qui n'ont pas de notion de canal, c'est toujours `true`. Voir **Regroupement et file d'attente des canaux**.

## Groupes de rubans

`pix::StripGroup<N>` présente plusieurs rubans indépendants comme un seul espace d'index continu à base zéro, afin qu'une animation puisse s'étendre sur plusieurs broches de données sans suivre les frontières à la main. Chaque ruban membre garde son propre tampon de pixels et est rafraîchi sur sa propre broche.

```cpp
template <uint8_t STRIP_COUNT>
class StripGroup;                          // derives from StripGroupInterface

using StripGroupHandler = StripGroupInterface*;
```

### Constructeurs

```cpp
StripGroup();                                       // empty
StripGroup(const StripHandler (&strips)[STRIP_COUNT]); // from an array of strip pointers
```

Remplis un groupe en passant un tableau de `StripHandler` (pointeurs vers des rubans existants). Les rubans sont référencés, pas copiés, ils doivent donc survivre au groupe.

```cpp
Strip<30> a{2};
Strip<30> b{4};
StripHandler handlers[2] = { &a, &b };
StripGroup<2> group{handlers};
```

### Champs publics

| Champ | Type | Description |
| :---- | :--- | :---------- |
| `brightness` | `uint8_t` | Luminosité passée au `show()` de chaque ruban ; par défaut `128`. |

### Méthodes

```cpp
uint32_t length() const;        // cached total pixel count across all strips
uint32_t recalcLength();        // recompute and cache the total; call after populating the group
void begin();                   // call begin() on every member strip
void clear();                   // clear every member strip's buffer
void show();                    // refresh every strip using the group `brightness`
void show(uint8_t bright);      // refresh every strip using `bright`
void showStrip(uint8_t strip);  // refresh only the strip at the given slot

void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to);
void fillFromTo(uint32_t color, uint32_t from, uint32_t to);
void fillFromTo(const Color& color, uint32_t from, uint32_t to);

LED& led(uint32_t i);           // pixel by global index (spans all strips)
const LED& led(uint32_t i) const;
LED& operator[](uint32_t i);    // equivalent to led(i)
const LED& operator[](uint32_t i) const;

StripHandler strip(uint8_t s);  // pointer to the member strip in slot s (nullptr if out of range)
```

Appelle `recalcLength()` une fois après avoir construit le groupe (et à nouveau si tu changes les rubans), pour que `length()`, `led()` et les remplissages d'intervalle connaissent la taille totale. Indexer au-delà du total renvoie le pixel factice partagé.

## Fonctions libres (bas niveau)

Elles opèrent directement sur un tampon brut `LED*` et sont ce que les classes de ruban appellent en interne. Utilise-les quand tu gères le tampon toi-même.

```cpp
void begin(uint8_t pin);
void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright = 255, bool scale = true);
void clear(LED* leds, uint16_t len);

void fill(LED* leds, uint16_t len, uint32_t color);
void fill(LED* leds, uint16_t len, const Color& color);
void fill(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b);
void fillFromTo(LED* leds, uint16_t len, uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, const Color& color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
```

| Paramètre | Description |
| :-------- | :---------- |
| `leds` | Pointeur vers le tampon de pixels. |
| `len` | Nombre de pixels dans le tampon. |
| `pin` | Broche de données. |
| `bright` | Luminosité (0–255) appliquée quand `scale` vaut `true`. |
| `scale` | Si `false`, `show()` transmet le tampon tel quel, sans mise à l'échelle de luminosité. |
| `from`, `to` | Intervalle de pixels fermé pour `fillFromTo`. |

`begin(pin)` configure la broche (et, sur ESP32, provisionne le canal RMT). `show(...)` effectue une transmission en imposant l'intervalle de reset WS2812B entre trames.

### Aides de couleur

```cpp
Color  hsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);
Color  kelvin(uint16_t temp);
ColorW hsvW(uint16_t hue, uint8_t sat = 255, uint8_t val = 255); // RGBW: LEDW, W = 0
ColorW kelvinW(uint16_t temp);                                   // RGBW: LEDW, W = 0
Color& gamma32(Color& color);   // gamma-correct in place, returns the same reference
uint32_t gamma32(uint32_t color); // gamma-correct a packed color
```

| Fonction | Description |
| :------- | :---------- |
| `hsv` | Convertit HSV en RGB. `hue` couvre tout le cercle des couleurs sur toute sa plage `0–65535` ; `sat` et `val` sont `0–255`. |
| `kelvin` | Approxime la couleur RGB d'un corps noir à `temp` kelvins (la table couvre environ 520–10000 K). |
| `hsvW` / `kelvinW` | Comme `hsv` / `kelvin` mais renvoient un `LEDW` (`W = 0`), pour l'usage avec `WStrip` sans aucune conversion. |
| `gamma32` | Applique une correction gamma perceptuelle — sur un `Color` en place ou sur une valeur `0xRRGGBB`. |

---

## Rafraîchissement asynchrone (ESP32 / RP2040)

Sur ESP32 et RP2040, la transmission est pilotée par le matériel (RMT / PIO+DMA), elle peut donc s'exécuter pendant que le CPU fait autre chose. Cela est exposé via le paramètre de template `ASYNC`, choisi par ruban :

| | `Strip<LEN>` (`ASYNC = true`, défaut) | `Strip<LEN, false>` |
| :-- | :-- | :-- |
| Tampons | tampon d'édition **+** tampon d'envoi (instantané) | tampon d'édition seul |
| RAM par ruban | `6 × LEN` octets | `3 × LEN` octets |
| `show()` | non bloquant : instantané+mise à l'échelle dans le tampon d'envoi, démarre le transfert, retourne | bloquant : retourne quand la trame est sur le fil |
| Plusieurs rubans | émettent **en parallèle** (chacun sur son canal) | un à la fois |
| Modifier les pixels juste après `show()` | toujours sûr (le transfert lit l'instantané) | sûr depuis une tâche ; les tâches concurrentes doivent être protégées par mutex |

```cpp
Strip<60> a{2};     // async
Strip<60> b{4};     // async

a.fill(...); b.fill(...);
a.show();           // starts A, returns immediately
b.show();           // starts B while A is still on the wire -> both transmit at once
a.wait(); b.wait(); // block until each frame is done
```

Comme `show()` prend un instantané dans le tampon d'envoi, tu peux continuer à modifier les pixels du ruban juste après son retour sans déchirer la trame en cours. `wait()` avant un second `show()` sur le *même* ruban est implicite (un ruban finit sa trame précédente avant de démarrer la suivante) ; des rubans différents se chevauchent librement. `busy()` interroge sans bloquer. Voir `examples/async_multistrip`.

`ASYNC` n'est disponible que sur ESP32/RP2040. Sur AVR/ESP8266 le paramètre n'existe pas (`Strip<LEN, false>` y est une erreur de compilation) : ceux-ci font du bit-banging avec les interruptions désactivées, donc `show()` est toujours bloquant et la trame ne peut pas être déchirée.

### Regroupement et file d'attente des canaux (ESP32 / RP2040)

Chaque broche active a besoin d'un canal matériel (un canal RMT sur ESP32 ; une machine à états PIO + un canal DMA sur RP2040), et chaque puce n'en a qu'une poignée : 8 sur un ESP32 classique, 4 sur S2/S3, 2 TX sur C3 (voir `SOC_RMT_CHANNELS_PER_GROUP` pour ta puce) ; 8 sur RP2040 (2 blocs PIO × 4 machines à états). Déclarer plus de rubans que ça ne pose aucun problème — ce qui se passe quand tu les utilises réellement dépend du pilote :

- **RP2040, et ESP32 sur l'ancien cœur `driver/rmt.h` (cœur Arduino 2.x / IDF 4) :** les canaux sont regroupés et mis en file d'attente. Un appel `show()`/`show()` asynchrone obtient le canal qu'il possède déjà, ou en réclame un libre, ou — une fois le pool plein — vole le canal **inactif** le moins récemment utilisé et le réaffecte à la nouvelle broche. La réaffectation est bon marché (seul le GPIO de sortie change ; aucun canal ni DMA n'est démonté) et ne touche jamais un canal encore en cours de transmission. Si tous les canaux sont occupés à cet instant précis, l'appel attend simplement (en cédant la main aux autres tâches) qu'un canal se libère — une courte pause ponctuelle, pas un échec.
- **ESP32 sur le nouveau cœur `driver/rmt_tx.h` (cœur Arduino 3.x / IDF 5+) :** pas de regroupement. Ce pilote n'a aucune API publique pour réaffecter un canal existant à un autre GPIO sans le supprimer et le recréer (avec son encodeur) — trop coûteux pour le faire de façon transparente à chaque trame — donc une fois que tous les canaux `SOC_RMT_CHANNELS_PER_GROUP` appartiennent à d'autres broches, les broches suivantes ne peuvent tout simplement pas en obtenir un. C'est une limitation du nouveau pilote d'Espressif, pas de cette bibliothèque : même FastLED contourne ça en allant au-delà de l'API RMT publique, directement dans la matrice GPIO — une charge de maintenance que cette bibliothèque évite délibérément.

Appelle `strip.ok()` pour vérifier si une broche a actuellement (ou peut obtenir) un canal. Sur IDF5, un `false` est durable (et aussi journalisé une fois par broche via `ESP_LOGE`) au lieu que le ruban ne s'allume jamais silencieusement ; sur RP2040/IDF4, un `false` passager signifie juste que tous les canaux sont occupés à l'instant — le prochain `show()` réussira quand même, simplement mis en file d'attente.

## Notes par plateforme

### ESP32 et FreeRTOS

Le pilote RMT ne désactive jamais les interruptions ni l'ordonnanceur pendant la transmission, donc les autres tâches — et le second cœur — continuent normalement. `examples/esp32_freertos_concurrency` le montre : une seconde tâche garde une cadence régulière pendant qu'un ruban est rafraîchi dans une boucle serrée.

À garder à l'esprit :

- **Pas de verrouillage interne.** Un ruban ou un groupe n'a aucune synchronisation intégrée. Avec un `Strip<LEN>` asynchrone tu peux modifier librement les pixels juste après `show()`, mais deux tâches touchant le *même* ruban doivent quand même être sérialisées. Avec `Strip<LEN, false>` (et sur AVR/ESP8266) une écriture concurrente pendant `show()` peut déchirer la trame — protège-la par mutex :

  ```cpp
  SemaphoreHandle_t stripLock = xSemaphoreCreateMutex();
  // ...
  xSemaphoreTake(stripLock, portMAX_DELAY);
  strip.fill(255, 0, 0);
  strip.show();
  xSemaphoreGive(stripLock);
  ```

- **Budget de canaux.** Un ESP32 classique a 8 canaux RMT (moins sur S2/S3/C3 — voir `SOC_RMT_CHANNELS_PER_GROUP` pour ta puce). Sur l'ancien cœur (IDF 4), davantage de broches que cela partagent le pool de façon transparente via une file d'attente ; sur le nouveau cœur (IDF 5+), c'est une limite stricte. Voir **Regroupement et file d'attente des canaux**.

Les deux générations du cœur arduino-esp32 sont prises en charge et choisies automatiquement : l'ancienne API `driver/rmt.h` (cœur 2.x / IDF 4) et la nouvelle API d'encodeur `driver/rmt_tx.h` (cœur 3.x / IDF 5). La bibliothèque elle-même n'alloue rien dynamiquement (l'encodeur/registre sont statiques) ; le SDK RMT alloue ses propres structures de canal une fois par broche au setup, ce qui est inhérent à l'usage du pilote.

### RP2040 / Raspberry Pi Pico

Vise le cœur arduino-pico (Earle Philhower) ; une machine à états PIO génère la forme d'onde WS2812B en matériel, cadencée par le PIO, donc sans besoin de verrou d'interruption. Deux chemins de transmission, tous deux sans allocation (aucun tas du tout) :

- **`Strip<LEN>` async :** un seul canal DMA envoie le tampon d'envoi pré-mis à l'échelle vers le FIFO PIO et retourne ; l'achèvement est interrogé dans `wait()`/`busy()`. Jusqu'à 8 rubans actifs en même temps (2 blocs PIO × 4 machines à états) ; au-delà, les canaux sont regroupés et mis en file d'attente — voir **Regroupement et file d'attente des canaux**.
- **Bloquant (`Strip<LEN, false>` et le `pix::show()` bas niveau) :** le CPU met à l'échelle chaque octet et alimente le FIFO directement — pas de tampon, pas de DMA. Le FIFO TX à 8 entrées (~80 µs) absorbe la gigue d'ordonnancement.

### ESP8266

Vise le cœur Arduino ESP8266. La forme d'onde est faite en bit-banging cadencé par le compteur de cycles Xtensa (CCOUNT), dérivé de la fréquence CPU réelle, donc correct à 80 comme à 160 MHz. N'importe quel GPIO fonctionne (broche 16 comprise), contrairement aux astuces I2S/UART qui figent sur une broche fixe. La luminosité est mise à l'échelle en ligne par octet, donc rien n'est alloué au rafraîchissement.

Les interruptions sont désactivées pendant la durée d'une trame (même compromis que le pilote AVR), car une interruption mal cadencée corromprait le signal. Le cœur est mono-thread (pas de FreeRTOS), donc cela ne bloque aucun ordonnanceur, mais garde des rubans de longueur raisonnable : les interruptions restent désactivées environ `pixels × 30 µs`, et les laisser désactivées très longtemps (des milliers de pixels) peut perturber le service Wi-Fi.

### ESP-IDF (sans Arduino)

La bibliothèque se compile aussi comme **composant ESP-IDF** natif, sans couche Arduino. Elle réutilise exactement le même pilote RMT et la même API `pix::` ; les quelques symboles Arduino dont dépendent les sources (`micros`, `pinMode`/`digitalWrite`, `PROGMEM`/`pgm_read_byte`) sont fournis par `pix_platform.h`, qui bascule entre `<Arduino.h>` et un shim pur IDF selon que `ARDUINO` et `ESP_PLATFORM` sont définis.

Rends le dépôt visible à ton projet comme composant — soit en le déposant sous `components/PixelPerfect/`, soit en pointant `EXTRA_COMPONENT_DIRS` dessus — puis requiers-le depuis ton composant applicatif :

```cmake
idf_component_register(SRCS "app_main.cpp" INCLUDE_DIRS "." REQUIRES PixelPerfect)
```

L'usage est identique à l'API Arduino ; seul le point d'entrée diffère (`app_main()` au lieu de `setup()`/`loop()`) :

```cpp
#include "PixelPerfect.h"
using namespace pix;

static Strip<39> strip{2};

extern "C" void app_main(void) {
    strip.begin();
    strip.brightness = 120;
    strip.fill(0, 255, 255);
    strip.show();
}
```

`examples/idf_basic` est un projet complet et compilable (`idf.py build`).

> **Note :** à partir d'ESP-IDF ≥ 5.3, les pilotes RMT et GPIO ont été séparés en composants distincts. Si un build signale un `driver/rmt_tx.h` ou `driver/gpio.h` manquant, ajoute `esp_driver_rmt esp_driver_gpio` au `REQUIRES` du composant dans le `CMakeLists.txt` racine.

## Configuration à la compilation

| Macro | Effet |
| :---- | :---- |
| `DISABLE_GAMMA_LUT` | Calcule la correction gamma en flottant au lieu de la table en flash (empreinte flash plus petite, plus lent). |
| `DISABLE_KELVIN_LUT` | Calcule `kelvin()` en flottant au lieu de la table en flash. |

## Exemples

| Sketch | Plateforme | Montre |
| :----- | :--------- | :----- |
| `examples/esp32_basic` | ESP32 (Arduino) | Usage minimal fill / show. |
| `examples/esp32_freertos_concurrency` | ESP32 (Arduino) | Une boucle de rafraîchissement à côté d'une autre tâche FreeRTOS sans la perturber. |
| `examples/esp8266_basic` | ESP8266 (Arduino) | Usage minimal fill / show. |
| `examples/rp2040_basic` | RP2040 (Arduino) | Usage minimal fill / show. |
| `examples/rgbw_basic` | toute (Arduino) | RGBW (`WStrip` / `LEDW`, SK6812) avec le canal blanc. |
| `examples/async_multistrip` | ESP32 / RP2040 | `show()` non bloquant, deux rubans émettant en même temps, `wait()` et une comparaison `Strip<N, false>`. |
| `examples/idf_basic` | ESP32 (ESP-IDF) | La même API depuis un projet `app_main()` pur. |

## Licence

Voir [LICENSE](LICENSE).

</details>

<details>
<summary><b>🇮🇹 Italiano</b></summary>

<br>

Una libreria C++ leggera per pilotare strisce LED indirizzabili **WS2812B** ("NeoPixel"). Punta a essere più semplice di FastLED evitando l'allocazione dinamica di memoria sul percorso critico: il tipo di striscia predefinito tiene il proprio buffer di pixel in un array a dimensione fissa, dimensionato staticamente.

Lo stesso codice sorgente si compila senza modifiche su più piattaforme, ognuna usando il miglior meccanismo di output offerto dall'hardware:

| Piattaforma | Framework | Meccanismo di output | Sorgente |
| :---------- | :-------- | :------------------- | :------- |
| AVR (es. ATmega2560) | Arduino | Bit-banging contato a cicli (assembly inline) | `atmega_show.cpp` |
| ESP32 (tutte le varianti) | Arduino **o** ESP-IDF | Periferica hardware RMT | `esp32_show.cpp` |
| ESP8266 | Arduino | Bit-banging temporizzato con CCOUNT | `esp8266_show.cpp` |
| RP2040 / Raspberry Pi Pico | Arduino (arduino-pico) | Macchina a stati PIO + DMA | `rp2040_show.cpp` |

Il backend corretto è scelto automaticamente a tempo di compilazione in base a macro di framework predefinite — non lo scegli mai a mano.

## Autore

- [@syzyf97](https://www.github.com/syzyf97)

## Caratteristiche

- Striscia a dimensione fissa senza allocazione (`Strip<N>`) più una variante opzionale su heap (`StripAllocate`).
- **RGB e RGBW** (es. SK6812) come due famiglie distinte e type-safe: `Strip`/`LED` e `WStrip`/`LEDW`.
- **Refresh non bloccante** su ESP32/RP2040 (`Strip<N>` è asincrona per default), così più strisce possono trasmettere insieme; `wait()`/`busy()` per sincronizzare.
- Scalatura della luminosità per striscia e per chiamata.
- Comode funzioni di colore: HSV, temperatura colore (Kelvin) e correzione gamma, per default basate su tabelle di lookup in flash.
- Raggruppamento di più strisce in un unico spazio di indici logico (`StripGroup` / `WStripGroup`).
- Compatibile con FreeRTOS su ESP32 e RP2040: un refresh non blocca mai lo scheduler né il secondo core.

## Formati di colore: RGB e RGBW

Due famiglie parallele e type-safe condividono un'unica implementazione:

| | Pixel | Striscia | Gruppo | Variante su heap |
| :-- | :-- | :-- | :-- | :-- |
| **RGB** (WS2812B) | `LED` (3 B) | `Strip<N>` | `StripGroup<M>` | `StripAllocate` |
| **RGBW** (SK6812) | `LEDW` (4 B) | `WStrip<N>` | `WStripGroup<M>` | `WStripAllocate` |

L'API è identica; RGBW aggiunge solo il canale bianco:

```cpp
WStrip<30> strip{2};
strip.begin();
strip.fill(0, 0, 0, 255);    // pure white through the W channel
strip.fill(255, 0, 0);       // red; the 3-argument fill sets W = 0
strip[10] = {0, 0, 255, 80}; // per-pixel R,G,B,W
strip[11] = hsvW(1000);      // RGBW color helpers -> LEDW (W = 0)
strip.fill(kelvinW(3000));   // no cast, no ambiguity
```

Un `LEDW` è un valore impacchettato di 4 byte; la sua forma a 32 bit è `0xRRGGBBWW` e `operator[]` indicizza `0=R, 1=G, 2=B, 3=W`. Usa le funzioni di colore RGBW `hsvW()` / `kelvinW()` — restituiscono un `LEDW` (con `W = 0`), così il codice RGBW non tocca mai il tipo `LED`. Per promuovere di proposito un `LED` esistente, chiama `led.toLEDW()` (non c'è conversione implicita RGB→RGBW: `strip[i] = hsv(x)` è un errore di compilazione voluto che ti indirizza a `hsvW` / `toLEDW`).

Le famiglie RGB e RGBW sono tipi distinti di proposito: un `WStripGroup` accetta solo `WStrip`, quindi mescolare i formati in un gruppo è un errore di compilazione. Le funzioni libere di basso livello rispecchiano quelle RGB — `fill(LEDW*, len, r, g, b, w)`, `fillFromTo(LEDW*, …)`, ecc. RGBW usa la stessa segnalazione di WS2812B — verifica il timing esatto sul tuo lotto di SK6812.

## Refresh asincrono (ESP32 / RP2040)

Su ESP32/RP2040 i template di striscia prendono un `bool ASYNC` opzionale (default `true`):

- `Strip<N>` / `WStrip<N>` — **async**: `show()` fa uno snapshot dei pixel in un buffer di invio per striscia, avvia il trasferimento e ritorna; puoi modificare subito la striscia. Chiama `wait()` prima di fare affidamento sul completamento, o `busy()` per fare polling. Strisce diverse trasmettono in parallelo. Costa un buffer statico aggiuntivo (`2 x pixel` byte in totale).
- `Strip<N, false>` / `WStrip<N, false>` — **bloccante**: nessun buffer di invio, `show()` ritorna quando il frame è sul filo. Metà della RAM, ma il trasferimento legge il buffer vivo, quindi modificarlo da un'altra task durante `show()` richiede un tuo mutex.

AVR/ESP8266 non hanno il parametro `ASYNC` (fanno bit-banging con gli interrupt disabilitati, quindi lì `Strip<N,false>` è un errore di compilazione) — `Strip<N>` è semplicemente bloccante.

## Installazione

**Arduino IDE** — copia il repository nella tua cartella `Arduino/libraries/` (o usa *Sketch → Include Library → Add .ZIP Library*), poi `#include <PixelPerfect.h>`.

**PlatformIO** — aggiungi la libreria a `lib_deps`, oppure metti il repository nella cartella `lib/` del progetto. Dichiara le architetture di destinazione con le solite impostazioni `platform`/`board`; il manifest della libreria pubblicizza `avr`, `esp32` e `rp2040`.

**ESP-IDF** — vedi **ESP-IDF (senza Arduino)** più sotto.

## Avvio rapido

```cpp
#include <PixelPerfect.h>

using namespace pix;

Strip<39> strip{2}; // 39 pixels, data on pin 2

void setup() {
    strip.begin();            // configure the pin / acquire the driver
    strip.brightness = 120;   // 0..255, applied on every show()

    strip.fill(0, 255, 255);  // set all pixels to cyan
    strip.show();             // push the buffer to the strip

    delay(2000);

    strip.clear();            // set all pixels to off
    strip[10] = {255, 255, 0}; // address a single pixel
    strip[10].b = 255;        // or a single color channel
    strip.show(255);          // push once at full brightness, leaving `brightness` untouched
}

void loop() {}
```

## Concetti di base

- **Modello di colore.** Un pixel è un `pix::LED` (alias `pix::Color`), un valore impacchettato di 3 byte. Lo usi come RGB — il costruttore prende `(r, g, b)`, `operator[]` indicizza `0=R, 1=G, 2=B`, e la forma a 32 bit è `0xRRGGBB`. I byte sono memorizzati internamente nell'ordine del filo WS2812B (GRB), quindi al refresh non avviene alcun riordino; è un dettaglio implementativo che puoi ignorare.
- **Luminosità.** Ogni striscia ha un campo pubblico `brightness` (0–255). `show()` scala ogni canale con un'approssimazione intera veloce, `canale * (brightness + 1) / 256`, quindi `255` è l'uscita piena e `0` è spento. `show(uint8_t)` permette di sovrascrivere la luminosità per un singolo refresh senza cambiare il valore memorizzato.
- **Reverse.** Costruire una striscia con `reverse = true` inverte l'ordine logico degli indici, così l'indice `0` corrisponde all'ultimo pixel fisico. Utile quando una striscia è montata testa-coda con un'altra.
- **Modello di memoria.** `Strip<N>` tiene i suoi `N` pixel in un array membro — niente heap, dimensione nota a compile time. `StripAllocate` li tiene sull'heap e può essere ridimensionata a runtime con `changeLength()`.
- **L'accesso fuori intervallo è sicuro.** Indicizzare una striscia oltre la sua lunghezza restituisce un riferimento a un pixel usa-e-getta condiviso invece di corrompere la memoria; le scritture su di esso non hanno effetto visibile.

---

# Riferimento API

Tutti i simboli vivono nel namespace `pix`.

## `pix::LED` (alias `pix::Color`)

Un singolo pixel: tre canali a 8 bit, impacchettati in esattamente 3 byte.

### Costruttori

```cpp
LED();                                 // black (0, 0, 0)
LED(uint8_t r, uint8_t g, uint8_t b);  // from channels
LED(uint32_t color);                   // from a packed 0xRRGGBB value
```

| Parametro | Descrizione |
| :-------- | :---------- |
| `r`, `g`, `b` | Canali rosso, verde e blu (0–255). |
| `color` | Colore impacchettato come `0xRRGGBB`. |

Il numero di argomenti sceglie il costruttore: **tre** numeri sono letti come canali (`LED{255, 0, 0}`), un **singolo** numero come colore impacchettato (`LED{0xFF0000}` è rosso). Ciò significa che un valore solo è sempre la forma impacchettata — `LED{0x20}` è `0x000020` (blu = 32), non rosso. La costruzione a uno o due argomenti non è fornita di proposito, così che un singolo intero non sia mai ambiguo tra i due costruttori.

### Operatori

```cpp
explicit operator uint32_t() const;        // -> 0xRRGGBB  (explicit: use (uint32_t)pixel)
uint8_t& operator[](uint8_t index);        // 0=R, 1=G, 2=B
LED& operator=(uint32_t color);            // assign from 0xRRGGBB
LED& operator=(const LED& other);
bool operator==(const LED& other) const;
bool operator==(uint32_t color) const;
bool operator!=(const LED& other) const;
bool operator!=(uint32_t color) const;
LEDW toLEDW() const;                        // promote to RGBW, W = 0
```

`operator[]` restituisce un riferimento al canale richiesto, quindi può essere letto o scritto (`pixel[0] = 255;`). L'indice è ridotto modulo 3, quindi `0`/`1`/`2` selezionano R/G/B e ogni valore maggiore va a capo invece di fallire. La conversione a `uint32_t` è **esplicita** — scrivi `uint32_t packed = (uint32_t)pixel;` — così un `LED` non diventa mai silenziosamente un intero (cosa che prima confliggeva con gli overload RGBW). Il pixel `LEDW` rispecchia questa API con un quarto canale `W` e una forma impacchettata `0xRRGGBBWW`.

## Strisce

Tutti i tipi di striscia derivano dall'interfaccia astratta `pix::StripInterface` e quindi condividono la stessa API. Sono fornite due implementazioni concrete.

### Tipi

```cpp
// AVR / ESP8266:
template <uint16_t LEN>
using Strip = StripTemplate<LEN>;

// ESP32 / RP2040: an extra opt-out ASYNC parameter (see "Asynchronous refresh"):
template <uint16_t LEN, bool ASYNC = true>
using Strip = StripTemplate<LEN, ASYNC>;

class StripAllocate;                       // pixels stored on the heap, resizable
using StripHandler = StripInterface*;      // pointer to any strip, used by StripGroup
```

`Strip<LEN>` si compila ovunque. `Strip<LEN, false>` è accettata solo su ESP32/RP2040 — il parametro `ASYNC` non è volutamente esposto su AVR/ESP8266 (vedi **Refresh asincrono**).

### Costruttori

```cpp
Strip<LEN>(uint8_t pin = 255, bool reverse = false);
Strip<LEN, false>(uint8_t pin = 255, bool reverse = false);   // ESP32/RP2040 only
StripAllocate(uint16_t pixelCount, uint8_t pin = 255, bool reverse = false);
StripAllocate();                           // empty; set length later with changeLength()
```

| Parametro | Descrizione |
| :-------- | :---------- |
| `LEN` / `pixelCount` | Numero di pixel della striscia. |
| `ASYNC` | *(solo ESP32/RP2040, default `true`.)* `true` = doppio buffer, `show()` non bloccante; `false` = buffer singolo, `show()` bloccante (metà della RAM). |
| `pin` | Pin dati. `255` significa "non assegnato" — impostalo dal costruttore o con `setPin()` prima di `begin()`. |
| `reverse` | Inverte l'ordine logico dei pixel. |

### Campi pubblici

| Campo | Tipo | Descrizione |
| :---- | :--- | :---------- |
| `brightness` | `uint8_t` | Luminosità applicata da `show()`; default `120`. |
| `reverse` | `bool` | Se l'indicizzazione logica è invertita. |

### Ciclo di vita

```cpp
void begin();
```
Prepara la striscia all'output: configura il pin dati e, su ESP32, acquisisce e configura il canale RMT legato a quel pin. Chiamalo una volta in `setup()` / prima del primo `show()`.

### Indirizzamento dei pixel

```cpp
LED& led(uint16_t i);
const LED& led(uint16_t i) const;
LED& operator[](uint16_t i);
const LED& operator[](uint16_t i) const;
```

Restituiscono un riferimento al pixel `i` (rispettando `reverse`). `led(i)` e `operator[](i)` sono equivalenti. Un indice `>= length()` restituisce un pixel fittizio condiviso, così l'accesso fuori intervallo non va in crash né influisce sulla striscia.

### Impostare i colori

```cpp
void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
void fillFromTo(uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(const Color& color, uint16_t from, uint16_t to);
```

`fill(...)` imposta ogni pixel a un solo colore. `fillFromTo(...)` imposta l'intervallo chiuso `[from, to]`. Le chiamate d'intervallo non fanno nulla se `from > to` o `to >= length()`.

### Aggiornamento

```cpp
void show();                // scale by `brightness`, then transmit
void show(uint8_t bright);  // scale by `bright` for this refresh only

// ESP32 / RP2040 only:
void wait();                // block until this strip's async frame has finished
bool busy();                // true while an async frame is still on the wire
```

Trasmette il buffer di pixel alla striscia fisica. `show()` usa il campo `brightness` della striscia; `show(bright)` usa il valore fornito senza modificare il campo.

Su una `Strip<LEN>` predefinita (asincrona) su ESP32/RP2040, `show()` ritorna appena il trasferimento è iniziato; usa `wait()`/`busy()` per sincronizzare. Su AVR/ESP8266, e su `Strip<LEN, false>`, `show()` blocca finché il frame non è interamente inviato. Vedi **Refresh asincrono**.

### Altro

```cpp
uint16_t length() const;    // pixel count
void clear();               // set every pixel to off (does not transmit)
void setPin(uint8_t p);
uint8_t getPin() const;
bool ok() const;             // ESP32 / RP2040: false se questo pin non ha un canale
void changeLength(uint16_t l);   // StripAllocate only: reallocate to l pixels
```

`clear()` aggiorna solo il buffer; chiama poi `show()` per spegnere la striscia fisica. `ok()` indica se il pin di questa striscia ha attualmente (o può ancora ottenere) un canale hardware; su AVR/ESP8266, che non hanno il concetto di canale, è sempre `true`. Vedi **Pooling e coda dei canali**.

## Gruppi di strisce

`pix::StripGroup<N>` presenta più strisce indipendenti come un unico spazio di indici continuo a base zero, così un'animazione può estendersi su più pin dati senza tracciare i confini a mano. Ogni striscia membro tiene il proprio buffer di pixel ed è aggiornata sul proprio pin.

```cpp
template <uint8_t STRIP_COUNT>
class StripGroup;                          // derives from StripGroupInterface

using StripGroupHandler = StripGroupInterface*;
```

### Costruttori

```cpp
StripGroup();                                       // empty
StripGroup(const StripHandler (&strips)[STRIP_COUNT]); // from an array of strip pointers
```

Popola un gruppo passando un array di `StripHandler` (puntatori a strisce esistenti). Le strisce sono referenziate, non copiate, quindi devono sopravvivere al gruppo.

```cpp
Strip<30> a{2};
Strip<30> b{4};
StripHandler handlers[2] = { &a, &b };
StripGroup<2> group{handlers};
```

### Campi pubblici

| Campo | Tipo | Descrizione |
| :---- | :--- | :---------- |
| `brightness` | `uint8_t` | Luminosità passata al `show()` di ogni striscia; default `128`. |

### Metodi

```cpp
uint32_t length() const;        // cached total pixel count across all strips
uint32_t recalcLength();        // recompute and cache the total; call after populating the group
void begin();                   // call begin() on every member strip
void clear();                   // clear every member strip's buffer
void show();                    // refresh every strip using the group `brightness`
void show(uint8_t bright);      // refresh every strip using `bright`
void showStrip(uint8_t strip);  // refresh only the strip at the given slot

void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to);
void fillFromTo(uint32_t color, uint32_t from, uint32_t to);
void fillFromTo(const Color& color, uint32_t from, uint32_t to);

LED& led(uint32_t i);           // pixel by global index (spans all strips)
const LED& led(uint32_t i) const;
LED& operator[](uint32_t i);    // equivalent to led(i)
const LED& operator[](uint32_t i) const;

StripHandler strip(uint8_t s);  // pointer to the member strip in slot s (nullptr if out of range)
```

Chiama `recalcLength()` una volta dopo aver costruito il gruppo (e di nuovo se scambi le strisce), così `length()`, `led()` e i riempimenti d'intervallo conoscono la dimensione totale. Indicizzare oltre il totale restituisce il pixel fittizio condiviso.

## Funzioni libere (basso livello)

Operano direttamente su un buffer grezzo `LED*` e sono ciò che le classi striscia chiamano internamente. Usale quando gestisci il buffer da solo.

```cpp
void begin(uint8_t pin);
void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright = 255, bool scale = true);
void clear(LED* leds, uint16_t len);

void fill(LED* leds, uint16_t len, uint32_t color);
void fill(LED* leds, uint16_t len, const Color& color);
void fill(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b);
void fillFromTo(LED* leds, uint16_t len, uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, const Color& color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
```

| Parametro | Descrizione |
| :-------- | :---------- |
| `leds` | Puntatore al buffer di pixel. |
| `len` | Numero di pixel nel buffer. |
| `pin` | Pin dati. |
| `bright` | Luminosità (0–255) applicata quando `scale` è `true`. |
| `scale` | Se `false`, `show()` trasmette il buffer così com'è, senza scalatura della luminosità. |
| `from`, `to` | Intervallo di pixel chiuso per `fillFromTo`. |

`begin(pin)` configura il pin (e, su ESP32, predispone il canale RMT). `show(...)` esegue una trasmissione, imponendo la pausa di reset WS2812B tra i frame.

### Funzioni di colore

```cpp
Color  hsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);
Color  kelvin(uint16_t temp);
ColorW hsvW(uint16_t hue, uint8_t sat = 255, uint8_t val = 255); // RGBW: LEDW, W = 0
ColorW kelvinW(uint16_t temp);                                   // RGBW: LEDW, W = 0
Color& gamma32(Color& color);   // gamma-correct in place, returns the same reference
uint32_t gamma32(uint32_t color); // gamma-correct a packed color
```

| Funzione | Descrizione |
| :------- | :---------- |
| `hsv` | Converte HSV in RGB. `hue` copre l'intera ruota dei colori su tutto il suo intervallo `0–65535`; `sat` e `val` sono `0–255`. |
| `kelvin` | Approssima il colore RGB di un corpo nero a `temp` kelvin (la tabella copre circa 520–10000 K). |
| `hsvW` / `kelvinW` | Come `hsv` / `kelvin` ma restituiscono un `LEDW` (`W = 0`), per l'uso con `WStrip` senza alcun cast. |
| `gamma32` | Applica la correzione gamma percettiva — a un `Color` sul posto o a un valore `0xRRGGBB`. |

---

## Refresh asincrono (ESP32 / RP2040)

Su ESP32 e RP2040 la trasmissione è pilotata dall'hardware (RMT / PIO+DMA), quindi può girare mentre la CPU fa altro. È esposto tramite il parametro di template `ASYNC`, scelto per striscia:

| | `Strip<LEN>` (`ASYNC = true`, default) | `Strip<LEN, false>` |
| :-- | :-- | :-- |
| Buffer | buffer di modifica **+** buffer di invio (snapshot) | solo buffer di modifica |
| RAM per striscia | `6 × LEN` byte | `3 × LEN` byte |
| `show()` | non bloccante: snapshot+scalatura nel buffer di invio, avvia il trasferimento, ritorna | bloccante: ritorna quando il frame è sul filo |
| Più strisce | trasmettono **in parallelo** (ognuna sul proprio canale) | una alla volta |
| Modificare i pixel subito dopo `show()` | sempre sicuro (il trasferimento legge lo snapshot) | sicuro da una task; le task concorrenti vanno protette con mutex |

```cpp
Strip<60> a{2};     // async
Strip<60> b{4};     // async

a.fill(...); b.fill(...);
a.show();           // starts A, returns immediately
b.show();           // starts B while A is still on the wire -> both transmit at once
a.wait(); b.wait(); // block until each frame is done
```

Poiché `show()` fa uno snapshot nel buffer di invio, puoi continuare a modificare i pixel della striscia subito dopo il suo ritorno senza strappare il frame in corso. `wait()` prima di un secondo `show()` sulla *stessa* striscia è implicito (una striscia finisce il frame precedente prima di iniziare il successivo); strisce diverse si sovrappongono liberamente. `busy()` fa polling senza bloccare. Vedi `examples/async_multistrip`.

`ASYNC` è disponibile solo su ESP32/RP2040. Su AVR/ESP8266 il parametro non esiste (`Strip<LEN, false>` lì è un errore di compilazione): fanno bit-banging con gli interrupt disabilitati, quindi `show()` è sempre bloccante e il frame non può essere strappato.

### Pooling e coda dei canali (ESP32 / RP2040)

Ogni pin attivo ha bisogno di un canale hardware (un canale RMT su ESP32; una macchina a stati PIO + un canale DMA su RP2040), e ogni chip ne ha solo una manciata: 8 su un ESP32 classico, 4 su S2/S3, 2 TX su C3 (vedi `SOC_RMT_CHANNELS_PER_GROUP` per il tuo chip); 8 su RP2040 (2 blocchi PIO × 4 macchine a stati). Dichiarare più strisce di quel numero va benissimo — ciò che accade quando le usi davvero dipende dal driver:

- **RP2040, ed ESP32 sul vecchio core `driver/rmt.h` (core Arduino 2.x / IDF 4):** i canali sono messi in pool e in coda. Una chiamata `show()`/`show()` asincrona ottiene il canale che possiede già, oppure ne rivendica uno libero, oppure — quando il pool è pieno — ruba il canale **inattivo** usato meno di recente e lo riassegna al nuovo pin. La riassegnazione è economica (cambia solo il GPIO di uscita; nessun canale o DMA viene smontato) e non tocca mai un canale ancora a metà trasmissione. Se in quell'istante tutti i canali sono occupati, la chiamata semplicemente attende (cedendo il passo ad altre task) finché uno non si libera — una breve pausa una tantum, non un fallimento.
- **ESP32 sul nuovo core `driver/rmt_tx.h` (core Arduino 3.x / IDF 5+):** nessun pooling. Questo driver non ha un'API pubblica per riassegnare un canale esistente a un altro GPIO senza eliminarlo e ricrearlo (insieme al suo encoder) — troppo costoso da fare in modo trasparente a ogni frame — quindi, quando tutti i canali `SOC_RMT_CHANNELS_PER_GROUP` appartengono già ad altri pin, i pin successivi semplicemente non possono ottenerne uno. È un limite del nuovo driver di Espressif, non di questa libreria: persino FastLED lo aggira accedendo oltre l'API RMT pubblica, direttamente alla GPIO matrix — un onere di manutenzione che questa libreria evita deliberatamente.

Chiama `strip.ok()` per verificare se un pin ha attualmente (o può ottenere) un canale. Su IDF5 un `false` è duraturo (e viene anche loggato una volta per pin tramite `ESP_LOGE`) invece che la striscia non si accenda mai silenziosamente; su RP2040/IDF4 un `false` momentaneo significa solo che tutti i canali sono occupati proprio ora — il prossimo `show()` riuscirà comunque, solo messo in coda.

## Note per piattaforma

### ESP32 e FreeRTOS

Il driver RMT non disabilita mai gli interrupt né lo scheduler durante la trasmissione, quindi le altre task — e il secondo core — continuano normalmente. `examples/esp32_freertos_concurrency` lo dimostra: una seconda task mantiene una cadenza costante mentre una striscia viene aggiornata in un ciclo stretto.

Da tenere presente:

- **Nessun blocco interno.** Una striscia o un gruppo non ha sincronizzazione integrata. Con una `Strip<LEN>` asincrona puoi modificare liberamente i pixel subito dopo `show()`, ma due task che toccano la *stessa* striscia vanno comunque serializzate. Con `Strip<LEN, false>` (e su AVR/ESP8266) una scrittura concorrente durante `show()` può strappare il frame — proteggila con un mutex:

  ```cpp
  SemaphoreHandle_t stripLock = xSemaphoreCreateMutex();
  // ...
  xSemaphoreTake(stripLock, portMAX_DELAY);
  strip.fill(255, 0, 0);
  strip.show();
  xSemaphoreGive(stripLock);
  ```

- **Budget di canali.** Un ESP32 classico ha 8 canali RMT (meno su S2/S3/C3 — vedi `SOC_RMT_CHANNELS_PER_GROUP` per il tuo chip). Sul vecchio core (IDF 4) più pin di quel numero condividono il pool in modo trasparente tramite una coda; sul nuovo core (IDF 5+) è un limite rigido. Vedi **Pooling e coda dei canali**.

Entrambe le generazioni del core arduino-esp32 sono supportate e scelte automaticamente: la vecchia API `driver/rmt.h` (core 2.x / IDF 4) e la nuova API encoder `driver/rmt_tx.h` (core 3.x / IDF 5). La libreria stessa non alloca nulla dinamicamente (encoder/registro sono statici); l'SDK RMT alloca le proprie strutture di canale una volta per pin al setup, cosa inerente all'uso del driver.

### RP2040 / Raspberry Pi Pico

Mira al core arduino-pico (Earle Philhower); una macchina a stati PIO genera la forma d'onda WS2812B in hardware, temporizzata dal PIO, quindi senza bisogno di blocco degli interrupt. Due percorsi di trasmissione, entrambi senza allocazione (nessun heap):

- **`Strip<LEN>` async:** un singolo canale DMA invia il buffer di invio pre-scalato al FIFO PIO e ritorna; il completamento è interrogato in `wait()`/`busy()`. Fino a 8 strisce attive contemporaneamente (2 blocchi PIO × 4 macchine a stati); oltre, i canali sono messi in pool e in coda — vedi **Pooling e coda dei canali**.
- **Bloccante (`Strip<LEN, false>` e il `pix::show()` di basso livello):** la CPU scala ogni byte e alimenta il FIFO direttamente — niente buffer, niente DMA. Il FIFO TX a 8 posizioni (~80 µs) assorbe il jitter di scheduling.

### ESP8266

Mira al core Arduino ESP8266. La forma d'onda è fatta in bit-banging temporizzata sul contatore di cicli Xtensa (CCOUNT), derivato dalla frequenza CPU reale, quindi corretta sia a 80 sia a 160 MHz. Funziona qualsiasi GPIO (pin 16 compreso), a differenza dei trucchi I2S/UART che vincolano a un pin fisso. La luminosità è scalata inline per byte, quindi al refresh non si alloca nulla.

Gli interrupt sono disabilitati per la durata di un frame (lo stesso compromesso del driver AVR), perché un interrupt mal temporizzato corromperebbe il segnale. Il core è a thread singolo (non FreeRTOS), quindi ciò non blocca alcuno scheduler, ma mantieni le strisce di lunghezza ragionevole: gli interrupt restano disabilitati per circa `pixel × 30 µs`, e tenerli disabilitati molto a lungo (migliaia di pixel) può disturbare il servizio Wi-Fi.

### ESP-IDF (senza Arduino)

La libreria si compila anche come **componente ESP-IDF** nativo, senza livello Arduino. Riusa esattamente lo stesso driver RMT e la stessa API `pix::`; i pochi simboli Arduino da cui dipendono i sorgenti (`micros`, `pinMode`/`digitalWrite`, `PROGMEM`/`pgm_read_byte`) sono forniti da `pix_platform.h`, che commuta tra `<Arduino.h>` e uno shim puro IDF a seconda che `ARDUINO` ed `ESP_PLATFORM` siano definiti.

Rendi il repository visibile al tuo progetto come componente — mettendolo sotto `components/PixelPerfect/` oppure puntandoci `EXTRA_COMPONENT_DIRS` — poi richiedilo dal tuo componente applicativo:

```cmake
idf_component_register(SRCS "app_main.cpp" INCLUDE_DIRS "." REQUIRES PixelPerfect)
```

L'uso è identico all'API Arduino; cambia solo il punto d'ingresso (`app_main()` invece di `setup()`/`loop()`):

```cpp
#include "PixelPerfect.h"
using namespace pix;

static Strip<39> strip{2};

extern "C" void app_main(void) {
    strip.begin();
    strip.brightness = 120;
    strip.fill(0, 255, 255);
    strip.show();
}
```

`examples/idf_basic` è un progetto completo e compilabile (`idf.py build`).

> **Nota:** da ESP-IDF ≥ 5.3 i driver RMT e GPIO sono stati separati in componenti distinti. Se una build segnala un `driver/rmt_tx.h` o `driver/gpio.h` mancante, aggiungi `esp_driver_rmt esp_driver_gpio` al `REQUIRES` del componente nel `CMakeLists.txt` radice.

## Configurazione a tempo di compilazione

| Macro | Effetto |
| :---- | :------ |
| `DISABLE_GAMMA_LUT` | Calcola la correzione gamma in virgola mobile invece della tabella in flash (impronta flash minore, più lento). |
| `DISABLE_KELVIN_LUT` | Calcola `kelvin()` in virgola mobile invece della tabella in flash. |

## Esempi

| Sketch | Piattaforma | Mostra |
| :----- | :---------- | :----- |
| `examples/esp32_basic` | ESP32 (Arduino) | Uso minimo di fill / show. |
| `examples/esp32_freertos_concurrency` | ESP32 (Arduino) | Un ciclo di refresh accanto a un'altra task FreeRTOS senza disturbarla. |
| `examples/esp8266_basic` | ESP8266 (Arduino) | Uso minimo di fill / show. |
| `examples/rp2040_basic` | RP2040 (Arduino) | Uso minimo di fill / show. |
| `examples/rgbw_basic` | qualsiasi (Arduino) | RGBW (`WStrip` / `LEDW`, SK6812) con il canale bianco. |
| `examples/async_multistrip` | ESP32 / RP2040 | `show()` non bloccante, due strisce che trasmettono insieme, `wait()` e un confronto con `Strip<N, false>`. |
| `examples/idf_basic` | ESP32 (ESP-IDF) | La stessa API da un progetto `app_main()` puro. |

## Licenza

Vedi [LICENSE](LICENSE).

</details>

<details>
<summary><b>🇪🇸 Español</b></summary>

<br>

Una biblioteca C++ ligera para controlar tiras de LED direccionables **WS2812B** ("NeoPixel"). Pretende ser más simple que FastLED evitando la asignación dinámica de memoria en la ruta crítica: el tipo de tira por defecto mantiene su búfer de píxeles en un arreglo de tamaño fijo, dimensionado estáticamente.

El mismo código fuente compila sin cambios en varias plataformas, cada una usando el mejor mecanismo de salida que ofrece el hardware:

| Plataforma | Framework | Mecanismo de salida | Fuente |
| :--------- | :-------- | :------------------ | :----- |
| AVR (p. ej. ATmega2560) | Arduino | Bit-banging contado por ciclos (ensamblador en línea) | `atmega_show.cpp` |
| ESP32 (todas las variantes) | Arduino **o** ESP-IDF | Periférico hardware RMT | `esp32_show.cpp` |
| ESP8266 | Arduino | Bit-banging temporizado por CCOUNT | `esp8266_show.cpp` |
| RP2040 / Raspberry Pi Pico | Arduino (arduino-pico) | Máquina de estados PIO + DMA | `rp2040_show.cpp` |

El backend correcto se elige automáticamente en tiempo de compilación a partir de macros de framework predefinidas — nunca lo eliges a mano.

## Autor

- [@syzyf97](https://www.github.com/syzyf97)

## Características

- Tira de tamaño fijo sin asignación (`Strip<N>`) más una variante opcional en el heap (`StripAllocate`).
- **RGB y RGBW** (p. ej. SK6812) como dos familias separadas y con seguridad de tipos: `Strip`/`LED` y `WStrip`/`LEDW`.
- **Refresco no bloqueante** en ESP32/RP2040 (`Strip<N>` es asíncrona por defecto), así varias tiras pueden transmitir a la vez; `wait()`/`busy()` para sincronizar.
- Escalado de brillo por tira y por llamada.
- Ayudantes de color prácticos: HSV, temperatura de color (Kelvin) y corrección gamma, por defecto basados en tablas de búsqueda en flash.
- Agrupación de varias tiras en un único espacio de índices lógico (`StripGroup` / `WStripGroup`).
- Compatible con FreeRTOS en ESP32 y RP2040: un refresco nunca bloquea el planificador ni el segundo núcleo.

## Formatos de color: RGB y RGBW

Dos familias paralelas y con seguridad de tipos comparten una sola implementación:

| | Píxel | Tira | Grupo | Variante en heap |
| :-- | :-- | :-- | :-- | :-- |
| **RGB** (WS2812B) | `LED` (3 B) | `Strip<N>` | `StripGroup<M>` | `StripAllocate` |
| **RGBW** (SK6812) | `LEDW` (4 B) | `WStrip<N>` | `WStripGroup<M>` | `WStripAllocate` |

La API es idéntica; RGBW solo añade el canal blanco:

```cpp
WStrip<30> strip{2};
strip.begin();
strip.fill(0, 0, 0, 255);    // pure white through the W channel
strip.fill(255, 0, 0);       // red; the 3-argument fill sets W = 0
strip[10] = {0, 0, 255, 80}; // per-pixel R,G,B,W
strip[11] = hsvW(1000);      // RGBW color helpers -> LEDW (W = 0)
strip.fill(kelvinW(3000));   // no cast, no ambiguity
```

Un `LEDW` es un valor empaquetado de 4 bytes; su forma de 32 bits es `0xRRGGBBWW` y `operator[]` indexa `0=R, 1=G, 2=B, 3=W`. Usa los ayudantes de color RGBW `hsvW()` / `kelvinW()` — devuelven un `LEDW` (con `W = 0`), de modo que el código RGBW nunca toca el tipo `LED`. Para promover a propósito un `LED` existente, llama a `led.toLEDW()` (no hay conversión implícita RGB→RGBW: `strip[i] = hsv(x)` es un error de compilación deliberado que te dirige a `hsvW` / `toLEDW`).

Las familias RGB y RGBW son tipos distintos a propósito: un `WStripGroup` solo acepta `WStrip`, así que mezclar formatos en un grupo es un error de compilación. Las funciones libres de bajo nivel reflejan las de RGB — `fill(LEDW*, len, r, g, b, w)`, `fillFromTo(LEDW*, …)`, etc. RGBW usa la misma señalización que WS2812B — verifica el timing exacto en tu lote de SK6812.

## Refresco asíncrono (ESP32 / RP2040)

En ESP32/RP2040 las plantillas de tira toman un `bool ASYNC` opcional (por defecto `true`):

- `Strip<N>` / `WStrip<N>` — **async**: `show()` toma una instantánea de los píxeles en un búfer de envío propio de la tira, inicia la transferencia y retorna; puedes editar la tira de inmediato. Llama a `wait()` antes de depender de la finalización, o `busy()` para sondear. Tiras distintas transmiten en paralelo. Cuesta un búfer estático adicional (`2 x píxeles` bytes en total).
- `Strip<N, false>` / `WStrip<N, false>` — **bloqueante**: sin búfer de envío, `show()` retorna cuando el fotograma está en el cable. La mitad de RAM, pero la transferencia lee el búfer vivo, así que editarlo desde otra tarea durante `show()` requiere tu propio mutex.

AVR/ESP8266 no tienen parámetro `ASYNC` (hacen bit-banging con las interrupciones deshabilitadas, así que allí `Strip<N,false>` es un error de compilación) — `Strip<N>` es simplemente bloqueante.

## Instalación

**Arduino IDE** — copia el repositorio en tu carpeta `Arduino/libraries/` (o usa *Sketch → Include Library → Add .ZIP Library*), luego `#include <PixelPerfect.h>`.

**PlatformIO** — añade la biblioteca a `lib_deps`, o coloca el repositorio en la carpeta `lib/` del proyecto. Declara la(s) arquitectura(s) objetivo con los ajustes habituales `platform`/`board`; el manifiesto de la biblioteca anuncia `avr`, `esp32` y `rp2040`.

**ESP-IDF** — véase **ESP-IDF (sin Arduino)** más abajo.

## Inicio rápido

```cpp
#include <PixelPerfect.h>

using namespace pix;

Strip<39> strip{2}; // 39 pixels, data on pin 2

void setup() {
    strip.begin();            // configure the pin / acquire the driver
    strip.brightness = 120;   // 0..255, applied on every show()

    strip.fill(0, 255, 255);  // set all pixels to cyan
    strip.show();             // push the buffer to the strip

    delay(2000);

    strip.clear();            // set all pixels to off
    strip[10] = {255, 255, 0}; // address a single pixel
    strip[10].b = 255;        // or a single color channel
    strip.show(255);          // push once at full brightness, leaving `brightness` untouched
}

void loop() {}
```

## Conceptos básicos

- **Modelo de color.** Un píxel es un `pix::LED` (alias `pix::Color`), un valor empaquetado de 3 bytes. Trabajas con él como RGB — el constructor toma `(r, g, b)`, `operator[]` indexa `0=R, 1=G, 2=B`, y la forma de 32 bits es `0xRRGGBB`. Los bytes se almacenan internamente en el orden del cable WS2812B (GRB), así que en el refresco no hay reordenamiento; es un detalle de implementación que puedes ignorar.
- **Brillo.** Cada tira tiene un campo público `brightness` (0–255). `show()` escala cada canal con una aproximación entera rápida, `canal * (brightness + 1) / 256`, así `255` es salida plena y `0` es apagado. `show(uint8_t)` permite sobrescribir el brillo para un solo refresco sin cambiar el valor almacenado.
- **Reverse.** Construir una tira con `reverse = true` invierte el orden lógico de índices, así el índice `0` corresponde al último píxel físico. Útil cuando una tira se monta extremo con extremo con otra.
- **Modelo de memoria.** `Strip<N>` guarda sus `N` píxeles en un arreglo miembro — sin heap, tamaño conocido en compilación. `StripAllocate` los guarda en el heap y puede redimensionarse en ejecución con `changeLength()`.
- **El acceso fuera de rango es seguro.** Indexar una tira más allá de su longitud devuelve una referencia a un píxel desechable compartido en vez de corromper memoria; las escrituras en él no tienen efecto visible.

---

# Referencia de la API

Todos los símbolos viven en el espacio de nombres `pix`.

## `pix::LED` (alias `pix::Color`)

Un solo píxel: tres canales de 8 bits, empaquetados en exactamente 3 bytes.

### Constructores

```cpp
LED();                                 // black (0, 0, 0)
LED(uint8_t r, uint8_t g, uint8_t b);  // from channels
LED(uint32_t color);                   // from a packed 0xRRGGBB value
```

| Parámetro | Descripción |
| :-------- | :---------- |
| `r`, `g`, `b` | Canales rojo, verde y azul (0–255). |
| `color` | Color empaquetado como `0xRRGGBB`. |

El número de argumentos elige el constructor: **tres** números se leen como canales (`LED{255, 0, 0}`), un **solo** número como color empaquetado (`LED{0xFF0000}` es rojo). Esto significa que un valor solo es siempre la forma empaquetada — `LED{0x20}` es `0x000020` (azul = 32), no rojo. La construcción de uno o dos argumentos no se ofrece a propósito, para que un solo entero nunca sea ambiguo entre los dos constructores.

### Operadores

```cpp
explicit operator uint32_t() const;        // -> 0xRRGGBB  (explicit: use (uint32_t)pixel)
uint8_t& operator[](uint8_t index);        // 0=R, 1=G, 2=B
LED& operator=(uint32_t color);            // assign from 0xRRGGBB
LED& operator=(const LED& other);
bool operator==(const LED& other) const;
bool operator==(uint32_t color) const;
bool operator!=(const LED& other) const;
bool operator!=(uint32_t color) const;
LEDW toLEDW() const;                        // promote to RGBW, W = 0
```

`operator[]` devuelve una referencia al canal solicitado, así que puede leerse o escribirse (`pixel[0] = 255;`). El índice se reduce módulo 3, así que `0`/`1`/`2` seleccionan R/G/B y cualquier valor mayor da la vuelta en vez de fallar. La conversión a `uint32_t` es **explícita** — escribe `uint32_t packed = (uint32_t)pixel;` — para que un `LED` nunca se convierta en silencio en un entero (lo que antes chocaba con las sobrecargas RGBW). El píxel `LEDW` refleja esta API con un cuarto canal `W` y una forma empaquetada `0xRRGGBBWW`.

## Tiras

Todos los tipos de tira derivan de la interfaz abstracta `pix::StripInterface` y por tanto comparten la misma API. Se ofrecen dos implementaciones concretas.

### Tipos

```cpp
// AVR / ESP8266:
template <uint16_t LEN>
using Strip = StripTemplate<LEN>;

// ESP32 / RP2040: an extra opt-out ASYNC parameter (see "Asynchronous refresh"):
template <uint16_t LEN, bool ASYNC = true>
using Strip = StripTemplate<LEN, ASYNC>;

class StripAllocate;                       // pixels stored on the heap, resizable
using StripHandler = StripInterface*;      // pointer to any strip, used by StripGroup
```

`Strip<LEN>` compila en todas partes. `Strip<LEN, false>` solo se acepta en ESP32/RP2040 — el parámetro `ASYNC` no se expone a propósito en AVR/ESP8266 (véase **Refresco asíncrono**).

### Constructores

```cpp
Strip<LEN>(uint8_t pin = 255, bool reverse = false);
Strip<LEN, false>(uint8_t pin = 255, bool reverse = false);   // ESP32/RP2040 only
StripAllocate(uint16_t pixelCount, uint8_t pin = 255, bool reverse = false);
StripAllocate();                           // empty; set length later with changeLength()
```

| Parámetro | Descripción |
| :-------- | :---------- |
| `LEN` / `pixelCount` | Número de píxeles de la tira. |
| `ASYNC` | *(solo ESP32/RP2040, por defecto `true`.)* `true` = doble búfer, `show()` no bloqueante; `false` = búfer único, `show()` bloqueante (la mitad de RAM). |
| `pin` | Pin de datos. `255` significa "sin asignar" — defínelo por el constructor o con `setPin()` antes de `begin()`. |
| `reverse` | Invierte el orden lógico de los píxeles. |

### Campos públicos

| Campo | Tipo | Descripción |
| :---- | :--- | :---------- |
| `brightness` | `uint8_t` | Brillo aplicado por `show()`; por defecto `120`. |
| `reverse` | `bool` | Si la indexación lógica está invertida. |

### Ciclo de vida

```cpp
void begin();
```
Prepara la tira para la salida: configura el pin de datos y, en ESP32, adquiere y configura el canal RMT ligado a ese pin. Llámalo una vez en `setup()` / antes del primer `show()`.

### Direccionamiento de píxeles

```cpp
LED& led(uint16_t i);
const LED& led(uint16_t i) const;
LED& operator[](uint16_t i);
const LED& operator[](uint16_t i) const;
```

Devuelven una referencia al píxel `i` (respetando `reverse`). `led(i)` y `operator[](i)` son equivalentes. Un índice `>= length()` devuelve un píxel ficticio compartido, así que el acceso fuera de rango ni se cae ni afecta a la tira.

### Establecer colores

```cpp
void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
void fillFromTo(uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(const Color& color, uint16_t from, uint16_t to);
```

`fill(...)` pone cada píxel en un solo color. `fillFromTo(...)` pone el rango cerrado `[from, to]`. Las llamadas de rango no hacen nada si `from > to` o `to >= length()`.

### Refresco

```cpp
void show();                // scale by `brightness`, then transmit
void show(uint8_t bright);  // scale by `bright` for this refresh only

// ESP32 / RP2040 only:
void wait();                // block until this strip's async frame has finished
bool busy();                // true while an async frame is still on the wire
```

Transmite el búfer de píxeles a la tira física. `show()` usa el campo `brightness` de la tira; `show(bright)` usa el valor dado sin modificar el campo.

En una `Strip<LEN>` por defecto (asíncrona) en ESP32/RP2040, `show()` retorna en cuanto ha comenzado la transferencia; usa `wait()`/`busy()` para sincronizar. En AVR/ESP8266, y en `Strip<LEN, false>`, `show()` bloquea hasta que el fotograma se envía por completo. Véase **Refresco asíncrono**.

### Otros

```cpp
uint16_t length() const;    // pixel count
void clear();               // set every pixel to off (does not transmit)
void setPin(uint8_t p);
uint8_t getPin() const;
bool ok() const;             // ESP32 / RP2040: false si este pin no tiene canal
void changeLength(uint16_t l);   // StripAllocate only: reallocate to l pixels
```

`clear()` solo actualiza el búfer; llama luego a `show()` para apagar la tira física. `ok()` indica si el pin de esta tira tiene actualmente (o todavía puede obtener) un canal de hardware; en AVR/ESP8266, que no tienen concepto de canal, siempre es `true`. Véase **Agrupación y cola de canales**.

## Grupos de tiras

`pix::StripGroup<N>` presenta varias tiras independientes como un único espacio de índices continuo de base cero, para que una animación pueda abarcar varios pines de datos sin seguir los límites a mano. Cada tira miembro mantiene su propio búfer de píxeles y se refresca en su propio pin.

```cpp
template <uint8_t STRIP_COUNT>
class StripGroup;                          // derives from StripGroupInterface

using StripGroupHandler = StripGroupInterface*;
```

### Constructores

```cpp
StripGroup();                                       // empty
StripGroup(const StripHandler (&strips)[STRIP_COUNT]); // from an array of strip pointers
```

Puebla un grupo pasando un arreglo de `StripHandler` (punteros a tiras existentes). Las tiras se referencian, no se copian, así que deben sobrevivir al grupo.

```cpp
Strip<30> a{2};
Strip<30> b{4};
StripHandler handlers[2] = { &a, &b };
StripGroup<2> group{handlers};
```

### Campos públicos

| Campo | Tipo | Descripción |
| :---- | :--- | :---------- |
| `brightness` | `uint8_t` | Brillo pasado al `show()` de cada tira; por defecto `128`. |

### Métodos

```cpp
uint32_t length() const;        // cached total pixel count across all strips
uint32_t recalcLength();        // recompute and cache the total; call after populating the group
void begin();                   // call begin() on every member strip
void clear();                   // clear every member strip's buffer
void show();                    // refresh every strip using the group `brightness`
void show(uint8_t bright);      // refresh every strip using `bright`
void showStrip(uint8_t strip);  // refresh only the strip at the given slot

void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to);
void fillFromTo(uint32_t color, uint32_t from, uint32_t to);
void fillFromTo(const Color& color, uint32_t from, uint32_t to);

LED& led(uint32_t i);           // pixel by global index (spans all strips)
const LED& led(uint32_t i) const;
LED& operator[](uint32_t i);    // equivalent to led(i)
const LED& operator[](uint32_t i) const;

StripHandler strip(uint8_t s);  // pointer to the member strip in slot s (nullptr if out of range)
```

Llama a `recalcLength()` una vez tras construir el grupo (y de nuevo si cambias las tiras), para que `length()`, `led()` y los rellenos de rango conozcan el tamaño total. Indexar más allá del total devuelve el píxel ficticio compartido.

## Funciones libres (bajo nivel)

Operan directamente sobre un búfer crudo `LED*` y son lo que las clases de tira llaman internamente. Úsalas cuando gestiones el búfer tú mismo.

```cpp
void begin(uint8_t pin);
void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright = 255, bool scale = true);
void clear(LED* leds, uint16_t len);

void fill(LED* leds, uint16_t len, uint32_t color);
void fill(LED* leds, uint16_t len, const Color& color);
void fill(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b);
void fillFromTo(LED* leds, uint16_t len, uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, const Color& color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
```

| Parámetro | Descripción |
| :-------- | :---------- |
| `leds` | Puntero al búfer de píxeles. |
| `len` | Número de píxeles del búfer. |
| `pin` | Pin de datos. |
| `bright` | Brillo (0–255) aplicado cuando `scale` es `true`. |
| `scale` | Si es `false`, `show()` transmite el búfer tal cual, sin escalado de brillo. |
| `from`, `to` | Rango de píxeles cerrado para `fillFromTo`. |

`begin(pin)` configura el pin (y, en ESP32, aprovisiona el canal RMT). `show(...)` realiza una transmisión, imponiendo la pausa de reset WS2812B entre fotogramas.

### Ayudantes de color

```cpp
Color  hsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);
Color  kelvin(uint16_t temp);
ColorW hsvW(uint16_t hue, uint8_t sat = 255, uint8_t val = 255); // RGBW: LEDW, W = 0
ColorW kelvinW(uint16_t temp);                                   // RGBW: LEDW, W = 0
Color& gamma32(Color& color);   // gamma-correct in place, returns the same reference
uint32_t gamma32(uint32_t color); // gamma-correct a packed color
```

| Función | Descripción |
| :------ | :---------- |
| `hsv` | Convierte HSV a RGB. `hue` cubre toda la rueda de color en todo su rango `0–65535`; `sat` y `val` son `0–255`. |
| `kelvin` | Aproxima el color RGB de un cuerpo negro a `temp` kelvin (la tabla cubre unos 520–10000 K). |
| `hsvW` / `kelvinW` | Como `hsv` / `kelvin` pero devuelven un `LEDW` (`W = 0`), para usar con `WStrip` sin ninguna conversión. |
| `gamma32` | Aplica corrección gamma perceptual — a un `Color` en el sitio o a un valor `0xRRGGBB`. |

---

## Refresco asíncrono (ESP32 / RP2040)

En ESP32 y RP2040 la transmisión la conduce el hardware (RMT / PIO+DMA), así que puede ejecutarse mientras la CPU hace otra cosa. Esto se expone mediante el parámetro de plantilla `ASYNC`, elegido por tira:

| | `Strip<LEN>` (`ASYNC = true`, por defecto) | `Strip<LEN, false>` |
| :-- | :-- | :-- |
| Búferes | búfer de edición **+** búfer de envío (instantánea) | solo búfer de edición |
| RAM por tira | `6 × LEN` bytes | `3 × LEN` bytes |
| `show()` | no bloqueante: instantánea+escalado en el búfer de envío, inicia la transferencia, retorna | bloqueante: retorna cuando el fotograma está en el cable |
| Varias tiras | transmiten **en paralelo** (cada una en su canal) | una a la vez |
| Editar píxeles justo tras `show()` | siempre seguro (la transferencia lee la instantánea) | seguro desde una tarea; las tareas concurrentes deben protegerse con mutex |

```cpp
Strip<60> a{2};     // async
Strip<60> b{4};     // async

a.fill(...); b.fill(...);
a.show();           // starts A, returns immediately
b.show();           // starts B while A is still on the wire -> both transmit at once
a.wait(); b.wait(); // block until each frame is done
```

Como `show()` toma una instantánea en el búfer de envío, puedes seguir editando los píxeles de la tira justo después de que retorne sin romper el fotograma en curso. `wait()` antes de un segundo `show()` en la *misma* tira es implícito (una tira termina su fotograma anterior antes de iniciar el siguiente); tiras distintas se solapan libremente. `busy()` sondea sin bloquear. Véase `examples/async_multistrip`.

`ASYNC` solo está disponible en ESP32/RP2040. En AVR/ESP8266 el parámetro no existe (`Strip<LEN, false>` allí es un error de compilación): estos hacen bit-banging con las interrupciones deshabilitadas, así que `show()` siempre es bloqueante y el fotograma no puede romperse.

### Agrupación y cola de canales (ESP32 / RP2040)

Cada pin activo necesita un canal de hardware (un canal RMT en ESP32; una máquina de estados PIO + un canal DMA en RP2040), y cada chip solo tiene un puñado: 8 en un ESP32 clásico, 4 en S2/S3, 2 TX en C3 (véase `SOC_RMT_CHANNELS_PER_GROUP` para tu chip); 8 en RP2040 (2 bloques PIO × 4 máquinas de estados). Declarar más tiras que eso no es problema — lo que ocurre cuando realmente las usas depende del driver:

- **RP2040, y ESP32 en el core antiguo `driver/rmt.h` (core Arduino 2.x / IDF 4):** los canales se agrupan y se ponen en cola. Una llamada a `show()`/`show()` asíncrono obtiene el canal que ya posee, o reclama uno libre, o — una vez lleno el conjunto — roba el canal **inactivo** menos usado recientemente y lo reasigna al nuevo pin. Reasignar es barato (solo cambia el GPIO de salida; no se desmonta ningún canal ni DMA) y nunca toca un canal aún en transmisión. Si en ese instante todos los canales están ocupados, la llamada simplemente espera (cediendo el turno a otras tareas) hasta que uno quede libre — una pausa breve y puntual, no un fallo.
- **ESP32 en el core nuevo `driver/rmt_tx.h` (core Arduino 3.x / IDF 5+):** sin agrupación. Este driver no tiene una API pública para reasignar un canal existente a otro GPIO sin eliminarlo y recrearlo (junto con su encoder) — demasiado costoso para hacerlo de forma transparente en cada fotograma — así que, una vez que todos los canales `SOC_RMT_CHANNELS_PER_GROUP` pertenecen a otros pines, los pines siguientes simplemente no pueden obtener uno. Esta es una limitación del driver más nuevo de Espressif, no de esta biblioteca: incluso FastLED lo evita accediendo más allá de la API RMT pública, directamente a la matriz GPIO — una carga de mantenimiento que esta biblioteca evita deliberadamente.

Llama a `strip.ok()` para comprobar si un pin tiene actualmente (o puede obtener) un canal. En IDF5 un `false` es duradero (y además se registra una vez por pin mediante `ESP_LOGE`) en lugar de que la tira nunca se encienda silenciosamente; en RP2040/IDF4 un `false` momentáneo solo significa que todos los canales están ocupados justo ahora — el siguiente `show()` funcionará igualmente, solo que en cola.

## Notas por plataforma

### ESP32 y FreeRTOS

El driver RMT nunca deshabilita las interrupciones ni el planificador durante la transmisión, así que otras tareas — y el segundo núcleo — siguen normalmente. `examples/esp32_freertos_concurrency` lo demuestra: una segunda tarea mantiene una cadencia constante mientras una tira se refresca en un bucle apretado.

Ten en cuenta:

- **Sin bloqueo interno.** Una tira o grupo no tiene sincronización integrada. Con una `Strip<LEN>` asíncrona puedes editar libremente los píxeles justo tras `show()`, pero dos tareas que tocan la *misma* tira aún deben serializarse. Con `Strip<LEN, false>` (y en AVR/ESP8266) una escritura concurrente durante `show()` puede romper el fotograma — protégela con mutex:

  ```cpp
  SemaphoreHandle_t stripLock = xSemaphoreCreateMutex();
  // ...
  xSemaphoreTake(stripLock, portMAX_DELAY);
  strip.fill(255, 0, 0);
  strip.show();
  xSemaphoreGive(stripLock);
  ```

- **Presupuesto de canales.** Un ESP32 clásico tiene 8 canales RMT (menos en S2/S3/C3 — véase `SOC_RMT_CHANNELS_PER_GROUP` para tu chip). En el core antiguo (IDF 4) más pines que eso comparten el conjunto de forma transparente mediante una cola; en el core nuevo (IDF 5+) es un límite estricto. Véase **Agrupación y cola de canales**.

Ambas generaciones del core arduino-esp32 están soportadas y se eligen automáticamente: la antigua API `driver/rmt.h` (core 2.x / IDF 4) y la nueva API de encoder `driver/rmt_tx.h` (core 3.x / IDF 5). La biblioteca en sí no asigna nada dinámicamente (el encoder/registro son estáticos); el SDK RMT asigna sus propias estructuras de canal una vez por pin en el setup, algo inherente al uso del driver.

### RP2040 / Raspberry Pi Pico

Apunta al core arduino-pico (Earle Philhower); una máquina de estados PIO genera la forma de onda WS2812B en hardware, temporizada por el PIO, así que no necesita bloqueo de interrupciones. Dos rutas de transmisión, ambas sin asignación (nada de heap):

- **`Strip<LEN>` async:** un solo canal DMA envía el búfer de envío pre-escalado al FIFO PIO y retorna; la finalización se sondea en `wait()`/`busy()`. Hasta 8 tiras activas a la vez (2 bloques PIO × 4 máquinas de estados); más allá de eso, los canales se agrupan y se ponen en cola — véase **Agrupación y cola de canales**.
- **Bloqueante (`Strip<LEN, false>` y el `pix::show()` de bajo nivel):** la CPU escala cada byte y alimenta el FIFO directamente — sin búfer, sin DMA. El FIFO TX de 8 entradas (~80 µs) absorbe el jitter de planificación.

### ESP8266

Apunta al core Arduino ESP8266. La forma de onda se hace con bit-banging temporizado contra el contador de ciclos Xtensa (CCOUNT), derivado de la frecuencia real de CPU, así que es correcta a 80 y a 160 MHz. Funciona cualquier GPIO (incluido el pin 16), a diferencia de los trucos I2S/UART que atan a un pin fijo. El brillo se escala en línea por byte, así que en el refresco no se asigna nada.

Las interrupciones se deshabilitan durante un fotograma (el mismo compromiso que el driver AVR), porque una interrupción mal temporizada corrompería la señal. El core es de un solo hilo (no FreeRTOS), así que esto no bloquea ningún planificador, pero mantén las tiras de longitud razonable: las interrupciones quedan deshabilitadas unos `píxeles × 30 µs`, y mantenerlas deshabilitadas mucho tiempo (miles de píxeles) puede perturbar el servicio Wi-Fi.

### ESP-IDF (sin Arduino)

La biblioteca también compila como **componente ESP-IDF** nativo, sin capa Arduino. Reutiliza exactamente el mismo driver RMT y la misma API `pix::`; los pocos símbolos de Arduino de los que dependen las fuentes (`micros`, `pinMode`/`digitalWrite`, `PROGMEM`/`pgm_read_byte`) los provee `pix_platform.h`, que alterna entre `<Arduino.h>` y un shim puro IDF según estén definidos `ARDUINO` y `ESP_PLATFORM`.

Haz visible el repositorio a tu proyecto como componente — colocándolo bajo `components/PixelPerfect/` o apuntando `EXTRA_COMPONENT_DIRS` a él — luego requiérelo desde tu componente de aplicación:

```cmake
idf_component_register(SRCS "app_main.cpp" INCLUDE_DIRS "." REQUIRES PixelPerfect)
```

El uso es idéntico a la API de Arduino; solo cambia el punto de entrada (`app_main()` en vez de `setup()`/`loop()`):

```cpp
#include "PixelPerfect.h"
using namespace pix;

static Strip<39> strip{2};

extern "C" void app_main(void) {
    strip.begin();
    strip.brightness = 120;
    strip.fill(0, 255, 255);
    strip.show();
}
```

`examples/idf_basic` es un proyecto completo y compilable (`idf.py build`).

> **Nota:** desde ESP-IDF ≥ 5.3 los drivers RMT y GPIO se separaron en componentes distintos. Si una compilación informa de un `driver/rmt_tx.h` o `driver/gpio.h` ausente, añade `esp_driver_rmt esp_driver_gpio` al `REQUIRES` del componente en el `CMakeLists.txt` raíz.

## Configuración en tiempo de compilación

| Macro | Efecto |
| :---- | :----- |
| `DISABLE_GAMMA_LUT` | Calcula la corrección gamma en coma flotante en vez de la tabla en flash (menor huella en flash, más lento). |
| `DISABLE_KELVIN_LUT` | Calcula `kelvin()` en coma flotante en vez de la tabla en flash. |

## Ejemplos

| Sketch | Plataforma | Muestra |
| :----- | :--------- | :------ |
| `examples/esp32_basic` | ESP32 (Arduino) | Uso mínimo de fill / show. |
| `examples/esp32_freertos_concurrency` | ESP32 (Arduino) | Un bucle de refresco junto a otra tarea FreeRTOS sin perturbarla. |
| `examples/esp8266_basic` | ESP8266 (Arduino) | Uso mínimo de fill / show. |
| `examples/rp2040_basic` | RP2040 (Arduino) | Uso mínimo de fill / show. |
| `examples/rgbw_basic` | cualquiera (Arduino) | RGBW (`WStrip` / `LEDW`, SK6812) con el canal blanco. |
| `examples/async_multistrip` | ESP32 / RP2040 | `show()` no bloqueante, dos tiras transmitiendo a la vez, `wait()` y una comparación con `Strip<N, false>`. |
| `examples/idf_basic` | ESP32 (ESP-IDF) | La misma API desde un proyecto `app_main()` puro. |

## Licencia

Véase [LICENSE](LICENSE).

</details>

<details>
<summary><b>🇷🇺 Русский</b></summary>

<br>

Лёгкая C++-библиотека для управления адресуемыми светодиодными лентами **WS2812B** («NeoPixel»). Она стремится быть проще, чем FastLED, избегая при этом динамического выделения памяти на горячем пути: тип ленты по умолчанию держит буфер пикселей в фиксированном, статически заданном массиве.

Один и тот же исходный код собирается без изменений на нескольких платформах, каждая использует лучший механизм вывода, который предлагает железо:

| Платформа | Фреймворк | Механизм вывода | Исходник |
| :-------- | :-------- | :-------------- | :------- |
| AVR (напр. ATmega2560) | Arduino | Бит-бэнг с подсчётом тактов (ассемблерные вставки) | `atmega_show.cpp` |
| ESP32 (все варианты) | Arduino **или** ESP-IDF | Аппаратная периферия RMT | `esp32_show.cpp` |
| ESP8266 | Arduino | Бит-бэнг по счётчику тактов CCOUNT | `esp8266_show.cpp` |
| RP2040 / Raspberry Pi Pico | Arduino (arduino-pico) | Конечный автомат PIO + DMA | `rp2040_show.cpp` |

Нужный бэкенд выбирается автоматически на этапе компиляции по предопределённым макросам фреймворка — вручную его никогда не выбираешь.

## Автор

- [@syzyf97](https://www.github.com/syzyf97)

## Возможности

- Лента фиксированного размера без выделения памяти (`Strip<N>`) плюс необязательный вариант на куче (`StripAllocate`).
- **RGB и RGBW** (напр. SK6812) как два отдельных типобезопасных семейства: `Strip`/`LED` и `WStrip`/`LEDW`.
- **Неблокирующее обновление** на ESP32/RP2040 (`Strip<N>` по умолчанию асинхронна), так что несколько лент могут передавать одновременно; `wait()`/`busy()` для синхронизации.
- Масштабирование яркости на ленту и на вызов.
- Удобные цветовые помощники: HSV, цветовая температура (кельвины) и гамма-коррекция, по умолчанию на таблицах во флеше.
- Группировка нескольких лент в одном логическом пространстве индексов (`StripGroup` / `WStripGroup`).
- Дружелюбно к FreeRTOS на ESP32 и RP2040: обновление никогда не тормозит планировщик или второе ядро.

## Форматы цвета: RGB и RGBW

Два параллельных типобезопасных семейства используют одну реализацию:

| | Пиксель | Лента | Группа | Вариант на куче |
| :-- | :-- | :-- | :-- | :-- |
| **RGB** (WS2812B) | `LED` (3 Б) | `Strip<N>` | `StripGroup<M>` | `StripAllocate` |
| **RGBW** (SK6812) | `LEDW` (4 Б) | `WStrip<N>` | `WStripGroup<M>` | `WStripAllocate` |

API одинаков; RGBW лишь добавляет белый канал:

```cpp
WStrip<30> strip{2};
strip.begin();
strip.fill(0, 0, 0, 255);    // pure white through the W channel
strip.fill(255, 0, 0);       // red; the 3-argument fill sets W = 0
strip[10] = {0, 0, 255, 80}; // per-pixel R,G,B,W
strip[11] = hsvW(1000);      // RGBW color helpers -> LEDW (W = 0)
strip.fill(kelvinW(3000));   // no cast, no ambiguity
```

`LEDW` — упакованное 4-байтовое значение; его 32-битная форма — `0xRRGGBBWW`, а `operator[]` индексирует `0=R, 1=G, 2=B, 3=W`. Используй цветовые помощники RGBW `hsvW()` / `kelvinW()` — они возвращают `LEDW` (с `W = 0`), поэтому RGBW-код никогда не касается типа `LED`. Чтобы намеренно повысить существующий `LED`, вызови `led.toLEDW()` (неявного преобразования RGB→RGBW нет: `strip[i] = hsv(x)` — это намеренная ошибка компиляции, направляющая к `hsvW` / `toLEDW`).

Семейства RGB и RGBW — намеренно разные типы: `WStripGroup` принимает только `WStrip`, поэтому смешивание форматов в одной группе — ошибка компиляции. Низкоуровневые свободные функции повторяют RGB-варианты — `fill(LEDW*, len, r, g, b, w)`, `fillFromTo(LEDW*, …)` и т. д. RGBW использует ту же сигнализацию, что и WS2812B — проверь точный тайминг на своей партии SK6812.

## Асинхронное обновление (ESP32 / RP2040)

На ESP32/RP2040 шаблоны ленты принимают необязательный `bool ASYNC` (по умолчанию `true`):

- `Strip<N>` / `WStrip<N>` — **async**: `show()` делает снимок пикселей в собственный буфер отправки ленты, запускает передачу и возвращается; ленту можно править сразу. Вызови `wait()` перед тем, как полагаться на завершение, или `busy()` для опроса. Разные ленты передают параллельно. Стоит одного дополнительного статического буфера (`2 x пиксели` байт всего).
- `Strip<N, false>` / `WStrip<N, false>` — **блокирующий**: без буфера отправки, `show()` возвращается, когда кадр уже на проводе. Вдвое меньше ОЗУ, но передача читает живой буфер, поэтому правка из другой задачи во время `show()` требует собственного мьютекса.

У AVR/ESP8266 нет параметра `ASYNC` (они бит-бэнгуют с выключенными прерываниями, поэтому там `Strip<N,false>` — ошибка компиляции) — `Strip<N>` просто блокирующая.

## Установка

**Arduino IDE** — скопируй репозиторий в свою папку `Arduino/libraries/` (или используй *Sketch → Include Library → Add .ZIP Library*), затем `#include <PixelPerfect.h>`.

**PlatformIO** — добавь библиотеку в `lib_deps` или положи репозиторий в папку `lib/` проекта. Объяви целевые архитектуры обычными настройками `platform`/`board`; манифест библиотеки объявляет `avr`, `esp32` и `rp2040`.

**ESP-IDF** — см. **ESP-IDF (без Arduino)** ниже.

## Быстрый старт

```cpp
#include <PixelPerfect.h>

using namespace pix;

Strip<39> strip{2}; // 39 pixels, data on pin 2

void setup() {
    strip.begin();            // configure the pin / acquire the driver
    strip.brightness = 120;   // 0..255, applied on every show()

    strip.fill(0, 255, 255);  // set all pixels to cyan
    strip.show();             // push the buffer to the strip

    delay(2000);

    strip.clear();            // set all pixels to off
    strip[10] = {255, 255, 0}; // address a single pixel
    strip[10].b = 255;        // or a single color channel
    strip.show(255);          // push once at full brightness, leaving `brightness` untouched
}

void loop() {}
```

## Основные понятия

- **Модель цвета.** Пиксель — это `pix::LED` (псевдоним `pix::Color`), упакованное 3-байтовое значение. Работаешь с ним как с RGB — конструктор принимает `(r, g, b)`, `operator[]` индексирует `0=R, 1=G, 2=B`, а 32-битная форма — `0xRRGGBB`. Байты хранятся внутри в порядке провода WS2812B (GRB), поэтому при обновлении переупорядочивания нет; это деталь реализации, которую можно игнорировать.
- **Яркость.** У каждой ленты есть публичное поле `brightness` (0–255). `show()` масштабирует каждый канал быстрым целочисленным приближением, `канал * (brightness + 1) / 256`, так что `255` — полный выход, а `0` — выключено. `show(uint8_t)` позволяет переопределить яркость для одного обновления, не меняя сохранённое значение.
- **Reverse.** Создание ленты с `reverse = true` переворачивает логический порядок индексов, так что индекс `0` соответствует физически последнему пикселю. Полезно, когда лента смонтирована встык с другой.
- **Модель памяти.** `Strip<N>` держит свои `N` пикселей в массиве-члене — без кучи, размер известен на компиляции. `StripAllocate` держит их в куче и может менять размер во время выполнения через `changeLength()`.
- **Доступ за границей безопасен.** Индексация ленты за её длину возвращает ссылку на общий «мусорный» пиксель вместо порчи памяти; записи в него не имеют видимого эффекта.

---

# Справочник API

Все символы живут в пространстве имён `pix`.

## `pix::LED` (псевдоним `pix::Color`)

Один пиксель: три 8-битных канала, упакованные ровно в 3 байта.

### Конструкторы

```cpp
LED();                                 // black (0, 0, 0)
LED(uint8_t r, uint8_t g, uint8_t b);  // from channels
LED(uint32_t color);                   // from a packed 0xRRGGBB value
```

| Параметр | Описание |
| :------- | :------- |
| `r`, `g`, `b` | Красный, зелёный и синий каналы (0–255). |
| `color` | Упакованный цвет в виде `0xRRGGBB`. |

Количество аргументов выбирает конструктор: **три** числа читаются как каналы (`LED{255, 0, 0}`), **одно** число — как упакованный цвет (`LED{0xFF0000}` — красный). Это значит, что одиночное значение всегда — упакованная форма: `LED{0x20}` — это `0x000020` (синий = 32), а не красный. Конструирование из одного или двух аргументов намеренно не предоставляется, чтобы одиночное целое никогда не было неоднозначным между двумя конструкторами.

### Операторы

```cpp
explicit operator uint32_t() const;        // -> 0xRRGGBB  (explicit: use (uint32_t)pixel)
uint8_t& operator[](uint8_t index);        // 0=R, 1=G, 2=B
LED& operator=(uint32_t color);            // assign from 0xRRGGBB
LED& operator=(const LED& other);
bool operator==(const LED& other) const;
bool operator==(uint32_t color) const;
bool operator!=(const LED& other) const;
bool operator!=(uint32_t color) const;
LEDW toLEDW() const;                        // promote to RGBW, W = 0
```

`operator[]` возвращает ссылку на запрошенный канал, поэтому его можно читать или писать (`pixel[0] = 255;`). Индекс приводится по модулю 3, так что `0`/`1`/`2` выбирают R/G/B, а любое большее значение оборачивается, а не падает. Преобразование в `uint32_t` — **явное**: пиши `uint32_t packed = (uint32_t)pixel;` — чтобы `LED` никогда молча не превращался в целое (что раньше конфликтовало с перегрузками RGBW). Пиксель `LEDW` повторяет этот API с четвёртым каналом `W` и упакованной формой `0xRRGGBBWW`.

## Ленты

Все типы лент наследуются от абстрактного `pix::StripInterface` и потому имеют общий API. Предоставлены две конкретные реализации.

### Типы

```cpp
// AVR / ESP8266:
template <uint16_t LEN>
using Strip = StripTemplate<LEN>;

// ESP32 / RP2040: an extra opt-out ASYNC parameter (see "Asynchronous refresh"):
template <uint16_t LEN, bool ASYNC = true>
using Strip = StripTemplate<LEN, ASYNC>;

class StripAllocate;                       // pixels stored on the heap, resizable
using StripHandler = StripInterface*;      // pointer to any strip, used by StripGroup
```

`Strip<LEN>` собирается везде. `Strip<LEN, false>` принимается только на ESP32/RP2040 — параметр `ASYNC` намеренно не открыт на AVR/ESP8266 (см. **Асинхронное обновление**).

### Конструкторы

```cpp
Strip<LEN>(uint8_t pin = 255, bool reverse = false);
Strip<LEN, false>(uint8_t pin = 255, bool reverse = false);   // ESP32/RP2040 only
StripAllocate(uint16_t pixelCount, uint8_t pin = 255, bool reverse = false);
StripAllocate();                           // empty; set length later with changeLength()
```

| Параметр | Описание |
| :------- | :------- |
| `LEN` / `pixelCount` | Число пикселей в ленте. |
| `ASYNC` | *(только ESP32/RP2040, по умолчанию `true`.)* `true` = двойная буферизация, неблокирующий `show()`; `false` = один буфер, блокирующий `show()` (вдвое меньше ОЗУ). |
| `pin` | Пин данных. `255` означает «не назначен» — задай его через конструктор или `setPin()` до `begin()`. |
| `reverse` | Перевернуть логический порядок пикселей. |

### Публичные поля

| Поле | Тип | Описание |
| :--- | :-- | :------- |
| `brightness` | `uint8_t` | Яркость, применяемая `show()`; по умолчанию `120`. |
| `reverse` | `bool` | Перевёрнута ли логическая индексация. |

### Жизненный цикл

```cpp
void begin();
```
Готовит ленту к выводу: настраивает пин данных и, на ESP32, захватывает и настраивает канал RMT, привязанный к этому пину. Вызови один раз в `setup()` / перед первым `show()`.

### Адресация пикселей

```cpp
LED& led(uint16_t i);
const LED& led(uint16_t i) const;
LED& operator[](uint16_t i);
const LED& operator[](uint16_t i) const;
```

Возвращают ссылку на пиксель `i` (с учётом `reverse`). `led(i)` и `operator[](i)` эквивалентны. Индекс `>= length()` возвращает общий фиктивный пиксель, так что доступ за границей не падает и не влияет на ленту.

### Задание цветов

```cpp
void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
void fillFromTo(uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(const Color& color, uint16_t from, uint16_t to);
```

`fill(...)` задаёт каждому пикселю один цвет. `fillFromTo(...)` задаёт замкнутый диапазон `[from, to]`. Диапазонные вызовы ничего не делают, если `from > to` или `to >= length()`.

### Обновление

```cpp
void show();                // scale by `brightness`, then transmit
void show(uint8_t bright);  // scale by `bright` for this refresh only

// ESP32 / RP2040 only:
void wait();                // block until this strip's async frame has finished
bool busy();                // true while an async frame is still on the wire
```

Передаёт буфер пикселей на физическую ленту. `show()` использует поле `brightness` ленты; `show(bright)` использует переданное значение, не меняя поля.

На стандартной (асинхронной) `Strip<LEN>` на ESP32/RP2040 `show()` возвращается, как только передача началась; используй `wait()`/`busy()` для синхронизации. На AVR/ESP8266 и на `Strip<LEN, false>` `show()` блокирует, пока кадр не отправлен полностью. См. **Асинхронное обновление**.

### Прочее

```cpp
uint16_t length() const;    // pixel count
void clear();               // set every pixel to off (does not transmit)
void setPin(uint8_t p);
uint8_t getPin() const;
bool ok() const;             // ESP32 / RP2040: false, если у этого пина нет канала
void changeLength(uint16_t l);   // StripAllocate only: reallocate to l pixels
```

`clear()` обновляет только буфер; затем вызови `show()`, чтобы погасить физическую ленту. `ok()` сообщает, есть ли у пина этой ленты сейчас (или он ещё может получить) аппаратный канал; на AVR/ESP8266, у которых нет понятия канала, всегда `true`. См. **Пул и очередь каналов**.

## Группы лент

`pix::StripGroup<N>` представляет несколько независимых лент как одно непрерывное пространство индексов с нуля, чтобы анимация могла охватывать несколько пинов данных без ручного отслеживания границ. Каждая лента-член держит собственный буфер пикселей и обновляется на своём пине.

```cpp
template <uint8_t STRIP_COUNT>
class StripGroup;                          // derives from StripGroupInterface

using StripGroupHandler = StripGroupInterface*;
```

### Конструкторы

```cpp
StripGroup();                                       // empty
StripGroup(const StripHandler (&strips)[STRIP_COUNT]); // from an array of strip pointers
```

Заполни группу, передав массив `StripHandler` (указателей на существующие ленты). Ленты именно указываются, а не копируются, поэтому должны пережить группу.

```cpp
Strip<30> a{2};
Strip<30> b{4};
StripHandler handlers[2] = { &a, &b };
StripGroup<2> group{handlers};
```

### Публичные поля

| Поле | Тип | Описание |
| :--- | :-- | :------- |
| `brightness` | `uint8_t` | Яркость, передаваемая в `show()` каждой ленты; по умолчанию `128`. |

### Методы

```cpp
uint32_t length() const;        // cached total pixel count across all strips
uint32_t recalcLength();        // recompute and cache the total; call after populating the group
void begin();                   // call begin() on every member strip
void clear();                   // clear every member strip's buffer
void show();                    // refresh every strip using the group `brightness`
void show(uint8_t bright);      // refresh every strip using `bright`
void showStrip(uint8_t strip);  // refresh only the strip at the given slot

void fill(uint8_t r, uint8_t g, uint8_t b);
void fill(uint32_t color);
void fill(const Color& color);
void fillFromTo(uint8_t r, uint8_t g, uint8_t b, uint32_t from, uint32_t to);
void fillFromTo(uint32_t color, uint32_t from, uint32_t to);
void fillFromTo(const Color& color, uint32_t from, uint32_t to);

LED& led(uint32_t i);           // pixel by global index (spans all strips)
const LED& led(uint32_t i) const;
LED& operator[](uint32_t i);    // equivalent to led(i)
const LED& operator[](uint32_t i) const;

StripHandler strip(uint8_t s);  // pointer to the member strip in slot s (nullptr if out of range)
```

Вызови `recalcLength()` один раз после сборки группы (и снова, если меняешь ленты), чтобы `length()`, `led()` и диапазонные заливки знали общий размер. Индексация за общий размер возвращает общий фиктивный пиксель.

## Свободные функции (низкоуровневые)

Работают напрямую с сырым буфером `LED*` и именно их классы лент вызывают внутри. Используй их, когда управляешь буфером сам.

```cpp
void begin(uint8_t pin);
void show(const LED* leds, uint16_t len, uint8_t pin, uint8_t bright = 255, bool scale = true);
void clear(LED* leds, uint16_t len);

void fill(LED* leds, uint16_t len, uint32_t color);
void fill(LED* leds, uint16_t len, const Color& color);
void fill(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b);
void fillFromTo(LED* leds, uint16_t len, uint32_t color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, const Color& color, uint16_t from, uint16_t to);
void fillFromTo(LED* leds, uint16_t len, uint8_t r, uint8_t g, uint8_t b, uint16_t from, uint16_t to);
```

| Параметр | Описание |
| :------- | :------- |
| `leds` | Указатель на буфер пикселей. |
| `len` | Число пикселей в буфере. |
| `pin` | Пин данных. |
| `bright` | Яркость (0–255), применяемая, когда `scale` равно `true`. |
| `scale` | Если `false`, `show()` передаёт буфер как есть, без масштабирования яркости. |
| `from`, `to` | Замкнутый диапазон пикселей для `fillFromTo`. |

`begin(pin)` настраивает пин (и, на ESP32, готовит канал RMT). `show(...)` выполняет одну передачу, выдерживая межкадровую паузу сброса WS2812B.

### Цветовые помощники

```cpp
Color  hsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);
Color  kelvin(uint16_t temp);
ColorW hsvW(uint16_t hue, uint8_t sat = 255, uint8_t val = 255); // RGBW: LEDW, W = 0
ColorW kelvinW(uint16_t temp);                                   // RGBW: LEDW, W = 0
Color& gamma32(Color& color);   // gamma-correct in place, returns the same reference
uint32_t gamma32(uint32_t color); // gamma-correct a packed color
```

| Функция | Описание |
| :------ | :------- |
| `hsv` | Преобразует HSV в RGB. `hue` покрывает весь цветовой круг на всём диапазоне `0–65535`; `sat` и `val` — `0–255`. |
| `kelvin` | Приближает RGB-цвет чёрного тела при `temp` кельвинах (таблица покрывает примерно 520–10000 K). |
| `hsvW` / `kelvinW` | То же, что `hsv` / `kelvin`, но возвращают `LEDW` (`W = 0`), для использования с `WStrip` без всякого приведения. |
| `gamma32` | Применяет перцептивную гамма-коррекцию — к `Color` на месте или к значению `0xRRGGBB`. |

---

## Асинхронное обновление (ESP32 / RP2040)

На ESP32 и RP2040 передачу ведёт железо (RMT / PIO+DMA), поэтому она может идти, пока CPU занят другим. Это открыто через параметр шаблона `ASYNC`, выбираемый на ленту:

| | `Strip<LEN>` (`ASYNC = true`, по умолчанию) | `Strip<LEN, false>` |
| :-- | :-- | :-- |
| Буферы | буфер правки **+** буфер отправки (снимок) | только буфер правки |
| ОЗУ на ленту | `6 × LEN` байт | `3 × LEN` байт |
| `show()` | неблокирующий: снимок+масштабирование в буфер отправки, запускает передачу, возвращается | блокирующий: возвращается, когда кадр на проводе |
| Несколько лент | передают **параллельно** (каждая на своём канале) | по одной |
| Правка пикселей сразу после `show()` | всегда безопасна (передача читает снимок) | безопасна из одной задачи; параллельные задачи нужно защищать мьютексом |

```cpp
Strip<60> a{2};     // async
Strip<60> b{4};     // async

a.fill(...); b.fill(...);
a.show();           // starts A, returns immediately
b.show();           // starts B while A is still on the wire -> both transmit at once
a.wait(); b.wait(); // block until each frame is done
```

Поскольку `show()` делает снимок в буфер отправки, можно продолжать править пиксели ленты сразу после его возврата, не разрывая текущий кадр. `wait()` перед вторым `show()` на *той же* ленте неявен (лента заканчивает предыдущий кадр перед началом следующего); разные ленты свободно накладываются. `busy()` опрашивает без блокировки. См. `examples/async_multistrip`.

`ASYNC` доступен только на ESP32/RP2040. На AVR/ESP8266 параметра нет (`Strip<LEN, false>` там — ошибка компиляции): они бит-бэнгуют с выключенными прерываниями, поэтому `show()` всегда блокирующий и кадр разорвать нельзя.

### Пул и очередь каналов (ESP32 / RP2040)

Каждому активному пину нужен один аппаратный канал (канал RMT на ESP32; конечный автомат PIO + канал DMA на RP2040), а у каждого чипа их всего горстка: 8 на классическом ESP32, 4 на S2/S3, 2 TX на C3 (см. `SOC_RMT_CHANNELS_PER_GROUP` для своего чипа); 8 на RP2040 (2 блока PIO × 4 автомата). Объявить лент больше этого числа — не проблема; то, что происходит, когда ты их реально используешь, зависит от драйвера:

- **RP2040 и ESP32 на старом ядре `driver/rmt.h` (ядро Arduino 2.x / IDF 4):** каналы объединены в пул и обслуживаются очередью. Вызов `show()`/асинхронного `show()` получает канал, которым уже владеет, либо занимает свободный, либо — когда пул исчерпан — забирает наименее недавно использованный **простаивающий** канал и перепривязывает его к новому пину. Перепривязка дешёвая (меняется только выходной GPIO; ни канал, ни DMA не разбираются) и никогда не трогает канал, ещё передающий данные. Если в этот момент все каналы заняты, вызов просто ждёт (уступая время другим задачам), пока какой-нибудь не освободится — короткая разовая задержка, а не отказ.
- **ESP32 на новом ядре `driver/rmt_tx.h` (ядро Arduino 3.x / IDF 5+):** без пула. У этого драйвера нет публичного API для перепривязки существующего канала к другому GPIO без его удаления и пересоздания (вместе с кодировщиком) — слишком дорого делать это прозрачно на каждом кадре, — так что, когда все каналы `SOC_RMT_CHANNELS_PER_GROUP` уже принадлежат другим пинам, следующие пины просто не могут получить канал. Это ограничение нового драйвера Espressif, а не этой библиотеки: даже FastLED обходит его, обращаясь в обход публичного API RMT прямо к GPIO matrix — к обслуживанию такого кода эта библиотека сознательно не стремится.

Вызови `strip.ok()`, чтобы проверить, есть ли у пина сейчас (или может ли он получить) канал. На IDF5 `false` устойчиво (и дополнительно логируется один раз на пин через `ESP_LOGE`) вместо того, чтобы лента молча никогда не загоралась; на RP2040/IDF4 временное `false` означает лишь то, что все каналы заняты прямо сейчас — следующий `show()` всё равно сработает, только встанет в очередь.

## Замечания по платформам

### ESP32 и FreeRTOS

Драйвер RMT никогда не выключает прерывания или планировщик во время передачи, поэтому другие задачи — и второе ядро — работают нормально. `examples/esp32_freertos_concurrency` это показывает: вторая задача держит ровный ритм, пока лента обновляется в плотном цикле.

Учти:

- **Нет внутренней блокировки.** У ленты или группы нет встроенной синхронизации. С асинхронной `Strip<LEN>` можно свободно править пиксели сразу после `show()`, но две задачи, трогающие *одну и ту же* ленту, всё равно нужно сериализовать. С `Strip<LEN, false>` (и на AVR/ESP8266) параллельная запись во время `show()` может разорвать кадр — защити мьютексом:

  ```cpp
  SemaphoreHandle_t stripLock = xSemaphoreCreateMutex();
  // ...
  xSemaphoreTake(stripLock, portMAX_DELAY);
  strip.fill(255, 0, 0);
  strip.show();
  xSemaphoreGive(stripLock);
  ```

- **Бюджет каналов.** У классического ESP32 8 каналов RMT (меньше на S2/S3/C3 — см. `SOC_RMT_CHANNELS_PER_GROUP` для своего чипа). На старом ядре (IDF 4) пины сверх этого числа прозрачно делят пул через очередь; на новом ядре (IDF 5+) это жёсткий предел. См. **Пул и очередь каналов**.

Обе поколения ядра arduino-esp32 поддерживаются и выбираются автоматически: старое API `driver/rmt.h` (ядро 2.x / IDF 4) и новое API кодировщика `driver/rmt_tx.h` (ядро 3.x / IDF 5). Сама библиотека ничего не выделяет динамически (кодировщик/реестр статические); SDK RMT выделяет собственные структуры канала один раз на пин при настройке, что присуще использованию драйвера.

### RP2040 / Raspberry Pi Pico

Нацелена на ядро arduino-pico (Earle Philhower); конечный автомат PIO генерирует форму сигнала WS2812B аппаратно, тактируемый PIO, поэтому блокировка прерываний не нужна. Два пути передачи, оба без выделения памяти (кучи нет вовсе):

- **Асинхронная `Strip<LEN>`:** один канал DMA стримит предварительно масштабированный буфер отправки в FIFO PIO и возвращается; завершение опрашивается в `wait()`/`busy()`. До 8 одновременно активных лент (2 блока PIO × 4 автомата); сверх этого каналы объединяются в пул и обслуживаются очередью — см. **Пул и очередь каналов**.
- **Блокирующий (`Strip<LEN, false>` и низкоуровневый `pix::show()`):** CPU масштабирует каждый байт и кормит FIFO напрямую — без буфера, без DMA. 8-элементное TX FIFO (~80 мкс) поглощает джиттер планирования.

### ESP8266

Нацелена на ядро Arduino для ESP8266. Форма сигнала бит-бэнгуется и тактируется по счётчику тактов Xtensa (CCOUNT), вычисляемому из реальной частоты CPU, поэтому корректна и на 80, и на 160 МГц. Работает любой GPIO (включая пин 16), в отличие от трюков I2S/UART, привязывающих к фиксированному пину. Яркость масштабируется на лету по байтам, поэтому при обновлении ничего не выделяется.

Прерывания выключены на время кадра (тот же компромисс, что и в драйвере AVR), потому что неправильно приуроченное прерывание испортило бы сигнал. Ядро однопоточное (не FreeRTOS), так что это не тормозит планировщик, но держи ленты разумной длины: прерывания выключены примерно на `пиксели × 30 мкс`, а держать их выключенными очень долго (тысячи пикселей) может нарушить обслуживание Wi-Fi.

### ESP-IDF (без Arduino)

Библиотека также собирается как нативный **компонент ESP-IDF**, без слоя Arduino. Она переиспользует тот же самый драйвер RMT и API `pix::`; немногие символы Arduino, на которые опираются исходники (`micros`, `pinMode`/`digitalWrite`, `PROGMEM`/`pgm_read_byte`), предоставляет `pix_platform.h`, который переключается между `<Arduino.h>` и чисто-IDF шимом в зависимости от того, определены ли `ARDUINO` и `ESP_PLATFORM`.

Сделай репозиторий видимым проекту как компонент — положи его под `components/PixelPerfect/` или наведи на него `EXTRA_COMPONENT_DIRS` — затем затребуй его из компонента приложения:

```cmake
idf_component_register(SRCS "app_main.cpp" INCLUDE_DIRS "." REQUIRES PixelPerfect)
```

Использование идентично API Arduino; отличается лишь точка входа (`app_main()` вместо `setup()`/`loop()`):

```cpp
#include "PixelPerfect.h"
using namespace pix;

static Strip<39> strip{2};

extern "C" void app_main(void) {
    strip.begin();
    strip.brightness = 120;
    strip.fill(0, 255, 255);
    strip.show();
}
```

`examples/idf_basic` — это полный собираемый проект (`idf.py build`).

> **Замечание:** начиная с ESP-IDF ≥ 5.3 драйверы RMT и GPIO разделены на отдельные компоненты. Если сборка сообщает об отсутствии `driver/rmt_tx.h` или `driver/gpio.h`, добавь `esp_driver_rmt esp_driver_gpio` в `REQUIRES` компонента в корневом `CMakeLists.txt`.

## Конфигурация на этапе компиляции

| Макрос | Эффект |
| :----- | :----- |
| `DISABLE_GAMMA_LUT` | Считает гамма-коррекцию в плавающей точке вместо таблицы во флеше (меньше след во флеше, медленнее). |
| `DISABLE_KELVIN_LUT` | Считает `kelvin()` в плавающей точке вместо таблицы во флеше. |

## Примеры

| Скетч | Платформа | Показывает |
| :---- | :-------- | :--------- |
| `examples/esp32_basic` | ESP32 (Arduino) | Минимальное использование fill / show. |
| `examples/esp32_freertos_concurrency` | ESP32 (Arduino) | Цикл обновления рядом с другой задачей FreeRTOS без её торможения. |
| `examples/esp8266_basic` | ESP8266 (Arduino) | Минимальное использование fill / show. |
| `examples/rp2040_basic` | RP2040 (Arduino) | Минимальное использование fill / show. |
| `examples/rgbw_basic` | любая (Arduino) | RGBW (`WStrip` / `LEDW`, SK6812) с белым каналом. |
| `examples/async_multistrip` | ESP32 / RP2040 | Неблокирующий `show()`, две ленты, передающие одновременно, `wait()` и сравнение с `Strip<N, false>`. |
| `examples/idf_basic` | ESP32 (ESP-IDF) | Тот же API из чистого проекта `app_main()`. |

## Лицензия

См. [LICENSE](LICENSE).

</details>



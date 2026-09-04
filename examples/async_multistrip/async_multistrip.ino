// Async multi-strip demo - ESP32 or RP2040 only (the ASYNC template parameter
// and wait()/busy() do not exist on AVR/ESP8266).
//
// Strip<N> is asynchronous by default: show() scales into a private send buffer,
// starts the transfer and returns immediately, so several strips (each on its
// own RMT channel / PIO+DMA channel) transmit at the same time. wait() blocks
// until that strip's frame has finished.
//
// Strip<N, false> opts out: it keeps no send buffer (half the RAM) and show()
// blocks until done - fine for a single task, but concurrent edits from another
// task must then be mutexed by the caller.

#include <PixelPerfect.h>

using namespace pix;

Strip<60> a{2};          // async (default)
Strip<60> b{4};          // async (default)
Strip<60, false> lean{5}; // blocking, no send buffer

void setup() {
    a.begin();
    b.begin();
    lean.begin();
    a.brightness = b.brightness = lean.brightness = 100;
}

void loop() {
    static uint16_t hue = 0;

    a.fill(hsv(hue));
    b.fill(hsv(hue + 20000));

    a.show();   // returns immediately
    b.show();   // starts while A is still on the wire -> both transmit at once
    // (other work could run here while both frames stream out)
    a.wait();   // block until A's frame is done
    b.wait();   // block until B's frame is done

    lean.fill(hsv(hue + 40000));
    lean.show(); // blocking: returns only once the frame is on the strip

    hue += 512;
    delay(20);
}

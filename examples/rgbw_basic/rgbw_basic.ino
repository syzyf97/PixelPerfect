#include <PixelPerfect.h>

using namespace pix;

WStrip<30> strip{2}; // 30 RGBW pixels (e.g. SK6812) on GPIO2

void setup() {
    strip.begin();
    strip.brightness = 120;

    strip.fill(0, 0, 0, 255);   // pure white through the dedicated W channel
    strip.show();

    delay(1000);
    strip.fill(255, 0, 0);      // red; the 3-arg fill leaves W = 0
    strip[10] = {0, 0, 255, 80}; // a single pixel: blue plus a touch of white
    strip.show();
}

void loop() {}

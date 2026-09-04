#include <PixelPerfect.h>

using namespace pix;

Strip<39> strip{2}; // 39 leds, GPIO2 (any GPIO works - bit-banged)

void setup() {
    strip.begin();
    strip.brightness = 120;
    strip.fill(0, 255, 255); // cyan
    strip.show();

    delay(2000);
    strip.clear();
    strip[10] = {255, 255, 0};
    strip.show(255);
}

void loop() {}

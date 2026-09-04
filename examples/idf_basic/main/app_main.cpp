// Pure ESP-IDF usage of PixelPerfect: no Arduino layer, entry point is
// app_main() instead of setup()/loop(). The pix:: API is identical to the
// Arduino examples; the WS2812 output goes through the same RMT driver.

#include "PixelPerfect.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace pix;

static Strip<39> strip{2}; // 39 leds on GPIO2, driven over RMT

extern "C" void app_main(void)
{
    strip.begin();          // acquires an RMT channel for the pin
    strip.brightness = 120;

    uint16_t hue = 0;
    uint16_t pos = 0;
    for (;;)
    {
        strip.clear();
        strip[pos] = hsv(hue);
        strip.show();

        pos = (pos + 1) % strip.length();
        hue += 1200;
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

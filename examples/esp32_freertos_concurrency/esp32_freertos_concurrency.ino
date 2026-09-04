// Demonstrates that Strip::show() does not stall the FreeRTOS scheduler:
// a second task, pinned to the SAME core as loop(), ticks on a steady
// cadence while the main task repeatedly updates a strip. If show()
// blocked interrupts/the scheduler (like the AVR bit-bang driver does),
// this task's logged deltas would show multi-millisecond spikes lining
// up with every show() call. With the RMT driver they should stay flat.

#include <PixelPerfect.h>

using namespace pix;

Strip<300> strip{2};

static void tickerTask(void*) {
    uint32_t last = millis();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        uint32_t now = millis();
        Serial.printf("ticker delta: %lu ms\n", (unsigned long)(now - last));
        last = now;
    }
}

void setup() {
    Serial.begin(115200);
    strip.begin();
    strip.brightness = 40;

    xTaskCreatePinnedToCore(tickerTask, "ticker", 2048, nullptr, 1, nullptr, xPortGetCoreID());
}

void loop() {
    static uint8_t hue = 0;
    strip.fill(pix::hsv(hue * 256));
    strip.show();
    hue++;
    delay(20);
}

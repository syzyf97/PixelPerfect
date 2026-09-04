# ESP-IDF component manifest for PixelPerfect.
#
# Only the sources that compile on ESP32 are listed; atmega_show.cpp and
# rp2040_show.cpp are fully guarded out on this target anyway. The WS2812
# driver in esp32_show.cpp is native ESP-IDF (RMT), the Arduino-only symbols
# it and LED.cpp use are supplied by pix_platform.h under pure IDF.
#
# NOTE: on ESP-IDF >= 5.3 the RMT TX / GPIO drivers live in the split
# components esp_driver_rmt / esp_driver_gpio. If a build reports a missing
# driver/rmt_tx.h or driver/gpio.h, extend REQUIRES with those two names.

idf_component_register(
    SRCS "LED.cpp" "Strip.cpp" "esp32_show.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)

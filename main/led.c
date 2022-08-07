
#include "led.h"
#include "led_strip.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "pixelstick-led";


static const rgb_t colors[] = {
    { .r = 0x0f, .g = 0x0f, .b = 0x0f },
    { .r = 0x00, .g = 0x00, .b = 0x2f },
    { .r = 0x00, .g = 0x2f, .b = 0x00 },
    { .r = 0x2f, .g = 0x00, .b = 0x00 },
    { .r = 0x00, .g = 0x00, .b = 0x00 },
};

#define COLORS_TOTAL (sizeof(colors) / sizeof(rgb_t))

void led_strip() {
    led_strip_t strip = {
        .type = LED_STRIP_WS2812,
        .length = 144,
        .gpio = 5,
        .buf = NULL,
#ifdef LED_STRIP_BRIGHTNESS
        .brightness = 255,
#endif
    };

    ESP_ERROR_CHECK(led_strip_init(&strip));

    size_t c = 0;
    while (1)
    {
        ESP_ERROR_CHECK(led_strip_fill(&strip, 0, strip.length, colors[c]));
        ESP_ERROR_CHECK(led_strip_flush(&strip));

        vTaskDelay(pdMS_TO_TICKS(1000));

        if (++c >= COLORS_TOTAL)
            c = 0;
    }
}

void start_led_strip() {
  xTaskCreatePinnedToCore(led_strip, "led_strip", configMINIMAL_STACK_SIZE * 5, 
                          NULL, 5, NULL, 1);
}
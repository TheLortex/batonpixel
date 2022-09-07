
#include "led.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/rmt.h"
#include "led_strip_esp.h"

static const char *TAG = "pixelstick-led";

enum led_state_kind {
    INIT,
    WAITING_FOR_CONNECTION,
    CONNECTED,
    BLACK,
    IN_ANIMATION
};

struct animation_block {
    unsigned int step;
    unsigned int width;
    unsigned int repeat;
    char* image_buffer;
};

struct waiting_for_connection_block {
    int step;
    bool direction_forward;
};

struct led_state {
    enum led_state_kind kind; 
    union 
    {
        unsigned int init_step;
        struct waiting_for_connection_block waiting_for_connection;
        unsigned int connected_step;
        struct animation_block animation;
    };
};

#define LED_NUMBER 144
#define CONFIG_EXAMPLE_RMT_TX_GPIO 5
#define RMT_TX_CHANNEL 0

void render(struct led_state* state, led_strip_t* strip) {
    int index;


    // ANIMATION
    char b, g, r;
    int base;

    switch (state->kind) {
        case INIT:
            index = state->init_step / 2;

            if (index < LED_NUMBER / 2) {
                ESP_ERROR_CHECK(strip->set_pixel(strip, LED_NUMBER / 2 + index, 12, 2, 10));
                ESP_ERROR_CHECK(strip->set_pixel(strip, LED_NUMBER / 2 - index, 12, 2, 10));
            } else {
                index -= LED_NUMBER / 2;
                ESP_ERROR_CHECK(strip->set_pixel(strip, LED_NUMBER / 2 + index, 0, 0, 0));
                ESP_ERROR_CHECK(strip->set_pixel(strip, LED_NUMBER / 2 - index, 0, 0, 0));
            }

            state->init_step++;
            if (state->init_step >= LED_NUMBER * 2) {
                ESP_LOGI(TAG, "Waiting for connection");
                state->kind = WAITING_FOR_CONNECTION;
                state->waiting_for_connection.step = 0;
                state->waiting_for_connection.direction_forward = true;
            }
            break;
        
        case WAITING_FOR_CONNECTION:

            index = state->waiting_for_connection.step;
            for (int i = 0; i < LED_NUMBER; i++) {
                if (abs(i * 10 - index) < 50) {
                    int dist = abs(i * 10 - index);
                    int ofs = 4 * (50 - dist);
                    ESP_ERROR_CHECK(strip->set_pixel(strip, i, ofs, 0, 0));
                } else {
                    ESP_ERROR_CHECK(strip->set_pixel(strip, i, 0, 0, 0));
                }
            }

            if (state->waiting_for_connection.direction_forward) {
                state->waiting_for_connection.step++;
            } else {
                state->waiting_for_connection.step--;
            }
            if (state->waiting_for_connection.step >= LED_NUMBER * 10) {
                state->waiting_for_connection.direction_forward = false;
                state->waiting_for_connection.step = LED_NUMBER * 10 - 1;
            } else if(state->waiting_for_connection.step < 0) {
                state->waiting_for_connection.direction_forward = true;
                state->waiting_for_connection.step = 0;
            }
            break;

        case CONNECTED:
            index = state->connected_step / 2;

            if (index < LED_NUMBER / 2) {
                ESP_ERROR_CHECK(strip->set_pixel(strip, LED_NUMBER / 2 + index, 2, 14, 4));
                ESP_ERROR_CHECK(strip->set_pixel(strip, LED_NUMBER / 2 - index, 2, 14, 4));
            } else {
                index -= LED_NUMBER / 2;
                ESP_ERROR_CHECK(strip->set_pixel(strip, LED_NUMBER / 2 + index, 0, 0, 0));
                ESP_ERROR_CHECK(strip->set_pixel(strip, LED_NUMBER / 2 - index, 0, 0, 0));
            }

            state->connected_step++;
            if (state->connected_step >= LED_NUMBER * 2) {
                ESP_LOGI(TAG, "Waiting for connection");
                state->kind = BLACK;
            }
            break;

        case BLACK:
            for (int i = 0; i < LED_NUMBER; i++) {
                ESP_ERROR_CHECK(strip->set_pixel(strip, i, 0, 0, 0));
            }
            break;

        case IN_ANIMATION:
            index = state->animation.step / 5;
            
            for (int i = 0; i < LED_NUMBER; i++) {
                // TODO: implement the padding thing.....
                base = i * 3 * state->animation.width + 
                        3 * index;
                b = state->animation.image_buffer[base];
                g = state->animation.image_buffer[base+1];
                r = state->animation.image_buffer[base+2];

                ESP_ERROR_CHECK(strip->set_pixel(strip, i, r / 3, g / 3, b / 3));
            }

            state->animation.step++;
            if (state->animation.step / 5 >= state->animation.width) {
                state->animation.step = 0;
                state->animation.repeat--;
            }

            if (state->animation.repeat == 0) {
                state->kind = BLACK;
            }

            break;


    }

}

#include "bmp.h"

void led_strip(void* arg) {
    QueueHandle_t led_event_queue = (QueueHandle_t) arg;
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(CONFIG_EXAMPLE_RMT_TX_GPIO, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(LED_NUMBER, (led_strip_dev_t)config.channel);
    led_strip_t *strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip) {
        ESP_LOGE(TAG, "install WS2812 driver failed");
    }
    // Clear LED strip (turn off all LEDs)
    ESP_ERROR_CHECK(strip->clear(strip, 100));
    // Init
    ESP_LOGI(TAG, "Init");

    struct led_state current_state = {
        .kind = INIT,
        .init_step = 0,
    };

    struct message event;

    while (true) {
        render(&current_state, strip);
        ESP_ERROR_CHECK(strip->refresh(strip, 100));

        vTaskDelay(pdMS_TO_TICKS(8)); // 120 Hz ?

        int rcv = xQueueReceive(led_event_queue, &event, 0);

        FILEHEADER* fh;
        INFOHEADER* ih;

        if (rcv) {
            switch (event.type) {
                case WIFI_CONNECTED:
                    current_state.kind = CONNECTED;
                    current_state.connected_step = 0;
                    break;
                case WIFI_DISCONNECTED:
                    current_state.kind = WAITING_FOR_CONNECTION;
                    current_state.waiting_for_connection.step = 0;
                    current_state.waiting_for_connection.direction_forward = true;
                    break;
                case STOP:
                    current_state.kind = BLACK;
                    break;
                case ANIMATE:

                    fh = (void*) event.http_animation.buffer;
                    ih = (void*) event.http_animation.buffer + sizeof(FILEHEADER);

                    printf("fM1 = %c, fM2 = %c, bfS = %u, un1 = %hu, un2 = %hu, iDO = %u\n", fh->fileMarker1, fh->fileMarker2, fh->bfSize, fh->unused1, fh->unused2, fh->imageDataOffset);
                    printf("w = %d, h = %d\n", ih->width, ih->height);

                    if (ih->height == 144) {

                        vTaskDelay(pdMS_TO_TICKS(5000)); 

                        current_state.kind = IN_ANIMATION;
                        current_state.animation.step = 0;
                        current_state.animation.width = ih->width;
                        current_state.animation.repeat = event.http_animation.repeat;
                        current_state.animation.image_buffer = 
                            event.http_animation.buffer 
                            + sizeof(FILEHEADER) + sizeof(INFOHEADER);
                    } else {
                        ESP_LOGW(TAG, "Wrong image height");
                    }

                    break;
            }
            printf("MESSAGE: %d", event.type);
        }

    }
}

void start_led_strip(QueueHandle_t led_event_queue) {
    xTaskCreatePinnedToCore(led_strip, "led_strip", configMINIMAL_STACK_SIZE * 5, 
                            (void*) led_event_queue, 18, NULL, 1);
}
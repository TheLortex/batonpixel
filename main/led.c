
#include "led.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "led_strip.h"
#include "bt.h"

#include "esp_timer.h"

static const char *TAG = "pixelstick-led";

enum led_state_kind
{
  INIT,
  WAITING_FOR_CONNECTION,
  CONNECTED,
  BLACK,
  IN_ANIMATION
};

struct animation_block
{
  unsigned int step;
  unsigned int max_position;
  unsigned int frames_per_pixel;
  bool streaming_ended;
  int ack_frame;
};

struct waiting_for_connection_block
{
  int step;
  bool direction_forward;
};

struct led_state
{
  enum led_state_kind kind;
  union
  {
    unsigned int init_step;
    struct waiting_for_connection_block waiting_for_connection;
    unsigned int connected_step;
    struct animation_block animation;
  };
};

#define CONFIG_EXAMPLE_RMT_TX_GPIO 5
#define RMT_TX_CHANNEL 0

#define MAX_COL 64
#define COLUMN_BYTES (LED_COUNT * 3)
#define PIXEL_BUFFER_SIZE (MAX_COL * COLUMN_BYTES)
static char pixel_buffer[PIXEL_BUFFER_SIZE];

void render(struct led_state *state, led_strip_handle_t strip)
{
  int index;

  // ANIMATION
  /* char b, g, r;
  int base; */

  switch (state->kind)
  {
  case INIT:
    index = state->init_step / 2;

    if (index < LED_COUNT / 2)
    {
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, LED_COUNT / 2 + index, 12, 2, 10));
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, LED_COUNT / 2 - index, 12, 2, 10));
    }
    else
    {
      index -= LED_COUNT / 2;
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, LED_COUNT / 2 + index, 0, 0, 0));
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, LED_COUNT / 2 - index, 0, 0, 0));
    }

    state->init_step++;
    if (state->init_step >= LED_COUNT * 2)
    {
      ESP_LOGI(TAG, "Waiting for connection");
      state->kind = WAITING_FOR_CONNECTION;
      state->waiting_for_connection.step = 0;
      state->waiting_for_connection.direction_forward = true;
    }
    break;

  case WAITING_FOR_CONNECTION:

    index = state->waiting_for_connection.step;
    for (int i = 0; i < LED_COUNT; i++)
    {
      if (abs(i * 10 - index) < 50)
      {
        int dist = abs(i * 10 - index);
        int ofs = 4 * (50 - dist);
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, ofs, 0, 0));
      }
      else
      {
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, 0, 0, 0));
      }
    }

    if (state->waiting_for_connection.direction_forward)
    {
      state->waiting_for_connection.step++;
    }
    else
    {
      state->waiting_for_connection.step--;
    }
    if (state->waiting_for_connection.step >= LED_COUNT * 10)
    {
      state->waiting_for_connection.direction_forward = false;
      state->waiting_for_connection.step = LED_COUNT * 10 - 1;
    }
    else if (state->waiting_for_connection.step < 0)
    {
      state->waiting_for_connection.direction_forward = true;
      state->waiting_for_connection.step = 0;
    }
    break;

  case CONNECTED:
    index = state->connected_step / 2;

    if (index < LED_COUNT / 2)
    {
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, LED_COUNT / 2 + index, 2, 14, 4));
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, LED_COUNT / 2 - index, 2, 14, 4));
    }
    else
    {
      index -= LED_COUNT / 2;
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, LED_COUNT / 2 + index, 0, 0, 0));
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, LED_COUNT / 2 - index, 0, 0, 0));
    }

    state->connected_step++;
    if (state->connected_step >= LED_COUNT * 2)
    {
      ESP_LOGI(TAG, "Ready");
      state->kind = BLACK;
    }
    break;

  case BLACK:
    for (int i = 0; i < LED_COUNT; i++)
    {
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, 0, 0, 0));
    }
    break;

  case IN_ANIMATION:;
    int pixel = state->animation.step / state->animation.frames_per_pixel;
    
    int column = pixel % MAX_COL;
    int planned_buffering_delta =
        (MAX_COL + state->animation.ack_frame - column) % MAX_COL;

    int actual_buffering_delta =
        (MAX_COL + state->animation.max_position - column) % MAX_COL;

    if (actual_buffering_delta < 8 && !state->animation.streaming_ended)
    {
      // ACK
      if (planned_buffering_delta < 8)
      {
        bt_ack(32 - planned_buffering_delta);
        state->animation.ack_frame = state->animation.ack_frame + 32 - planned_buffering_delta;
      }
    }
    else
    {
      int base;
      char r, g, b;

      if (planned_buffering_delta < 32 - 8 && !state->animation.streaming_ended)
      {
        // ACK
        bt_ack(32 - planned_buffering_delta);
        state->animation.ack_frame = state->animation.ack_frame + 32 - planned_buffering_delta;
      }

      if (column == state->animation.max_position % MAX_COL)
      {
        state->kind = BLACK;
      }
      else
      {
        for (int i = 0; i < LED_COUNT; i++)
        {
          base = (column * LED_COUNT + i) * 3;
          r = pixel_buffer[base];
          g = pixel_buffer[base + 1];
          b = pixel_buffer[base + 2];

          ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, r, g, b));
        }

        state->animation.step++;
      }
    }

    break;
  }
}

#include "bmp.h"

#include "rom/ets_sys.h"

void led_strip(void *arg)
{
  QueueHandle_t led_event_queue = (QueueHandle_t)arg;

  /* LED strip initialization with the GPIO and pixels number*/
  led_strip_config_t strip_config = {
      .strip_gpio_num = CONFIG_EXAMPLE_RMT_TX_GPIO, // The GPIO that connected to the LED strip's data line
      .max_leds = LED_COUNT,                        // The number of LEDs in the strip,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB,     // Pixel format of your LED strip
      .led_model = LED_MODEL_WS2812,                // LED strip model
      .flags.invert_out = false,                    // whether to invert the output signal (useful when your hardware has a level inverter)
  };

  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
      .resolution_hz = 10 * 1000 * 1000, // 10MHz
      .flags.with_dma = false,           // whether to enable the DMA feature
  };

  led_strip_handle_t strip;
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
  if (!strip)
  {
    ESP_LOGE(TAG, "install WS2812 driver failed");
  }
  // Clear LED strip (turn off all LEDs)
  ESP_ERROR_CHECK(led_strip_clear(strip));
  // Init
  ESP_LOGI(TAG, "Init");

  struct led_state current_state = {
      .kind = INIT,
      .init_step = 0,
  };

  struct message event;

  while (true)
  {
    render(&current_state, strip);
    ESP_ERROR_CHECK(led_strip_refresh(strip));

    // vTaskDelay(1);
    ets_delay_us(200);

    int rcv = xQueueReceive(led_event_queue, &event, 0);

    if (rcv)
    {
      switch (event.type)
      {
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
      case ANIMATE_BEGIN:
        ESP_LOGI(TAG, "Beginning animation !");
        current_state.kind = IN_ANIMATION;
        current_state.animation.max_position = 0;
        current_state.animation.step = 0;
        current_state.animation.frames_per_pixel = 125 / event.animation_speed;
        current_state.animation.streaming_ended = false;
        current_state.animation.ack_frame = 0;
        break;
      case ANIMATE_END:
        ESP_LOGI(TAG, "Ending animation !");
        current_state.animation.streaming_ended = true;
        break;
      case ANIMATE:
        int col_position = current_state.animation.max_position;
        current_state.animation.max_position = col_position + 1;

        ESP_LOGI(TAG,
                 "Feed %d (%d) (%d)",
                 col_position,
                 (current_state.animation.step / current_state.animation.frames_per_pixel),
                 current_state.animation.ack_frame);

        memcpy(&pixel_buffer[(col_position % MAX_COL) * COLUMN_BYTES], event.http_animation.buffer, COLUMN_BYTES);

        free(event.http_animation.buffer);

        break;
      }
    }
  }
}

void start_led_strip(QueueHandle_t led_event_queue)
{
  xTaskCreatePinnedToCore(led_strip, "led_strip", configMINIMAL_STACK_SIZE * 5,
                          (void *)led_event_queue, 18, NULL, 1);
}
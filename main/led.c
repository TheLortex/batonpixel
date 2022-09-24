
#include "led.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/rmt.h"
#include "led_strip_esp.h"
#include "bt.h"

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
  bool acked;
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

void render(struct led_state *state, led_strip_t *strip)
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
      ESP_ERROR_CHECK(strip->set_pixel(strip, LED_COUNT / 2 + index, 12, 2, 10));
      ESP_ERROR_CHECK(strip->set_pixel(strip, LED_COUNT / 2 - index, 12, 2, 10));
    }
    else
    {
      index -= LED_COUNT / 2;
      ESP_ERROR_CHECK(strip->set_pixel(strip, LED_COUNT / 2 + index, 0, 0, 0));
      ESP_ERROR_CHECK(strip->set_pixel(strip, LED_COUNT / 2 - index, 0, 0, 0));
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
        ESP_ERROR_CHECK(strip->set_pixel(strip, i, ofs, 0, 0));
      }
      else
      {
        ESP_ERROR_CHECK(strip->set_pixel(strip, i, 0, 0, 0));
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
      ESP_ERROR_CHECK(strip->set_pixel(strip, LED_COUNT / 2 + index, 2, 14, 4));
      ESP_ERROR_CHECK(strip->set_pixel(strip, LED_COUNT / 2 - index, 2, 14, 4));
    }
    else
    {
      index -= LED_COUNT / 2;
      ESP_ERROR_CHECK(strip->set_pixel(strip, LED_COUNT / 2 + index, 0, 0, 0));
      ESP_ERROR_CHECK(strip->set_pixel(strip, LED_COUNT / 2 - index, 0, 0, 0));
    }

    state->connected_step++;
    if (state->connected_step >= LED_COUNT * 2)
    {
      ESP_LOGI(TAG, "Waiting for connection");
      state->kind = BLACK;
    }
    break;

  case BLACK:
    for (int i = 0; i < LED_COUNT; i++)
    {
      ESP_ERROR_CHECK(strip->set_pixel(strip, i, 0, 0, 0));
    }
    break;

  case IN_ANIMATION:;
    int column = (state->animation.step / state->animation.frames_per_pixel) % MAX_COL;
    int buffering_delta =
        (MAX_COL + state->animation.max_position - column) % MAX_COL;

    if (buffering_delta < 16 && !state->animation.streaming_ended)
    {
      // ACK to obtain more data;
      if (state->animation.acked == false)
      {
        // ACK
        bt_ack();
        state->animation.acked = true;
      }
    }
    else
    {
      int base;
      char r, g, b;

      if (state->animation.acked == false && buffering_delta < 32)
      {
        // ACK
        bt_ack();
        state->animation.acked = true;
      }

      if (column == state->animation.max_position)
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

          ESP_ERROR_CHECK(strip->set_pixel(strip, i, r, g, b));
        }

        state->animation.step++;
      }
    }

    break;
  }
}

#include "bmp.h"

void led_strip(void *arg)
{
  QueueHandle_t led_event_queue = (QueueHandle_t)arg;
  rmt_config_t config = RMT_DEFAULT_CONFIG_TX(CONFIG_EXAMPLE_RMT_TX_GPIO, RMT_TX_CHANNEL);
  // set counter clock to 40MHz
  config.clk_div = 2;

  ESP_ERROR_CHECK(rmt_config(&config));
  ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

  // install ws2812 driver
  led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(LED_COUNT, (led_strip_dev_t)config.channel);
  led_strip_t *strip = led_strip_new_rmt_ws2812(&strip_config);
  if (!strip)
  {
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

  while (true)
  {
    render(&current_state, strip);
    ESP_ERROR_CHECK(strip->refresh(strip, 100));

    vTaskDelay(pdMS_TO_TICKS(8)); // 120 Hz ?

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
        break;
      case ANIMATE_END:
        current_state.animation.streaming_ended = true;
        break;
      case ANIMATE:;
        int col_position = current_state.animation.max_position;
        current_state.animation.max_position = (col_position + 1) % MAX_COL;

        ESP_LOGI(TAG,
                 "Feed %d (%d)",
                 col_position,
                 (current_state.animation.step / 5) % MAX_COL);

        memcpy(&pixel_buffer[col_position * COLUMN_BYTES], event.http_animation.buffer, COLUMN_BYTES);

        current_state.animation.acked = false;

        break;
      }
      printf("MESSAGE: %d", event.type);
    }
  }
}

void start_led_strip(QueueHandle_t led_event_queue)
{
  xTaskCreatePinnedToCore(led_strip, "led_strip", configMINIMAL_STACK_SIZE * 5,
                          (void *)led_event_queue, 18, NULL, 1);
}
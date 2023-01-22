#ifndef __COMMON_H_
#define __COMMON_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "esp_log.h"
#include <string.h>

#define LED_CORE 1
#define NET_CORE 0

enum message_type
{
  WIFI_CONNECTED,
  WIFI_DISCONNECTED,
  STOP,
  ANIMATE,
  ANIMATE_BEGIN,
  ANIMATE_END
};

struct http_animation_block
{
  int width;
  char *buffer;
};

struct message
{
  enum message_type type;
  union
  {
    struct http_animation_block http_animation;
    int animation_speed;
    bool animate_end_aborted;
  };
};

#endif
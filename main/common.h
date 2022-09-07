#ifndef __COMMON_H_
#define __COMMON_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "esp_log.h"
#include <string.h>

#define LED_CORE 1
#define NET_CORE 0

enum message_type {
  WIFI_CONNECTED,
  WIFI_DISCONNECTED,
  STOP,
  ANIMATE
};

struct http_animation_block {
  unsigned int repeat;
  char* buffer;
};

struct message {
  enum message_type type;
  union {
    struct http_animation_block http_animation;
  };
};

#endif
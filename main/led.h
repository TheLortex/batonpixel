#ifndef __LED_H_
#define __LED_H_

#include "common.h"

#define LED_COUNT 332

void start_led_strip(QueueHandle_t led_event_queue);

#endif
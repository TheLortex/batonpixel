#ifndef __BT_H_
#define __BT_H_

#include "common.h"
    
#define MSG_HEADER_HELLO 0
#define MSG_HEADER_PIXEL_COUNT 1
#define MSG_HEADER_PIXEL_DATA 2
#define MSG_HEADER_PIXEL_BEGIN 3
#define MSG_HEADER_PIXEL_ACK 4
#define MSG_HEADER_PIXEL_END 5

void bt_init(QueueHandle_t led_event_queue);
void bt_ack();

#endif

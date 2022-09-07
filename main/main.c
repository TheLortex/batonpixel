#include <stdio.h>
#include "nvs_flash.h"
#include "common.h"
#include "wifi.h"
#include "dns.h"
#include "led.h"
#include "http.h"

void app_main(void)
{
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  QueueHandle_t led_event_queue = xQueueCreate(16, sizeof(struct message));

  wifi_init_softap(led_event_queue);
  start_webserver(led_event_queue);
  start_mdns();
  start_led_strip(led_event_queue);
  start_dns_hijack();
}
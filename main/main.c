#include <stdio.h>
#include "nvs_flash.h"
#include "common.h"
#include "wifi.h"
#include "dns.h"
#include "led.h"
#include "http.h"

void app_main(void)
{
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_init_softap();
  start_webserver();
  start_mdns();
  start_led_strip();
  start_dns_hijack();
}
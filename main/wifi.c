#include "common.h"
#include "wifi.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "pixelstick-wifi";

static void event_handler(void* led_event_queue, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  struct message event;

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
    event.type = WIFI_CONNECTED;
    xQueueSend((QueueHandle_t) led_event_queue, &event, 100);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    event.type = WIFI_DISCONNECTED;
    xQueueSend((QueueHandle_t) led_event_queue, &event, 100);
  } 
}


void wifi_init_softap(QueueHandle_t led_event_queue) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();


  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.wifi_task_core_id = NET_CORE;
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_config = {
    .ap = {
      .ssid = "pixelstick",
      .ssid_len = strlen("pixelstick"),
      .channel = 1,
      .password = "batonpixel",
      .max_connection = 2,
      .authmode = WIFI_AUTH_WPA2_PSK,
    }
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  esp_event_handler_instance_t instance_any_id;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &event_handler,
                                                      (void*)led_event_queue,
                                                      &instance_any_id));
  // configure IP
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
	tcpip_adapter_ip_info_t info;
	memset(&info, 0, sizeof(info));
	IP4_ADDR(&info.ip, 192, 168, 0, 1);
	IP4_ADDR(&info.gw, 192, 168, 0, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

  ESP_LOGI(TAG, "Wifi ready");
}

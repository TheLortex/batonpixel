#include "common.h"
#include "dns.h"
#include "mdns.h"

static const char *TAG = "pixelstick-dns";

void start_mdns() {
  ESP_ERROR_CHECK( mdns_init() );
  ESP_ERROR_CHECK( mdns_hostname_set("pixelstick"));
  ESP_ERROR_CHECK( mdns_instance_name_set("pixelstick"));
  ESP_ERROR_CHECK( mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
  ESP_LOGI(TAG, "MDNS ready");
}

#define DNS_HIJACK_SRV_HEADER_SIZE           12
#define DNS_HIJACK_SRV_QUESTION_MAX_LENGTH   50

typedef struct __attribute__((packed)) dns_header_t {
	uint16_t ID;
	uint8_t  RD       :1;
	uint8_t  TC       :1;
	uint8_t  AA       :1;
	uint8_t  OPCODE   :4;
	uint8_t  QR       :1;
	uint8_t  RCODE    :4;
	uint8_t  Z        :3;
	uint8_t  RA       :1;
	uint16_t QDCOUNT;
	uint16_t ANCOUNT;
	uint16_t NSCOUNT;
	uint16_t ARCOUNT;
} dns_header_t;

typedef struct __attribute__((packed)) dns_hijack_answer_t {
	uint16_t NAME;
	uint16_t TYPE;
	uint16_t CLASS;
	uint32_t TTL;
	uint16_t RDLENGTH;
	uint32_t RDATA;
} dns_hijack_answer_t;

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <byteswap.h>

static void dns_hijack_srv_task() {
    uint8_t rx_buffer[128];

    ip4_addr_t resolve_ip;
    inet_pton(AF_INET, "192.168.0.1", &resolve_ip);

    for(;;) {
        struct sockaddr_in dest_addr;

        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(53);

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

        if(sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket");
            break;
        }

        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr*) &dest_addr, sizeof(dest_addr));

        if(err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind");
            break;
        }
        
        ESP_LOGI(TAG, "Listening...");

        for(;;) {
            struct sockaddr_in source_addr;
            socklen_t socklen = sizeof(source_addr);

            memset(rx_buffer, 0x00, sizeof(rx_buffer));
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr*) &source_addr, &socklen);

            if(len < 0) {
                ESP_LOGE(TAG, "revfrom failed");
                break;
            }

            if(len > DNS_HIJACK_SRV_HEADER_SIZE + DNS_HIJACK_SRV_QUESTION_MAX_LENGTH) {
                ESP_LOGW(TAG, "Received more data [%d] than expected. Ignoring.", len);
                continue;
            }

            // Nul termination. To prevent pointer escape
            rx_buffer[sizeof(rx_buffer) - 1] = '\0';

            dns_header_t *header = (dns_header_t*) rx_buffer;

            header->QR      = 1;
            header->OPCODE  = 0;
            header->AA      = 1;
            header->RCODE   = 0;
            header->TC      = 0;
            header->Z       = 0;
            header->RA      = 0;
            header->ANCOUNT = header->QDCOUNT;
            header->NSCOUNT = 0;
            header->ARCOUNT = 0;

            // ptr points to the beginning of the QUESTION
            uint8_t *ptr = rx_buffer + sizeof(dns_header_t);

            // Jump over QNAME
            while(*ptr++);

            // Jump over QTYPE
            ptr += 2;

            // Jump over QCLASS
            ptr += 2;

            dns_hijack_answer_t *answer = (dns_hijack_answer_t*) ptr;

            answer->NAME     = __bswap_16(0xC00C);
            answer->TYPE     = __bswap_16(1);
            answer->CLASS    = __bswap_16(1);
            answer->TTL      = 0;
            answer->RDLENGTH = __bswap_16(4);
            answer->RDATA    = resolve_ip.addr;

            // Jump over ANSWER
            ptr += sizeof(dns_hijack_answer_t);

            int err = sendto(sock, rx_buffer, ptr - rx_buffer, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));

            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending");
                break;
            }

            taskYIELD();
        }

        if(sock != -1) {
            ESP_LOGE(TAG, "DNS UH");
        }
    }
}

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void start_dns_hijack() {
  xTaskCreatePinnedToCore(dns_hijack_srv_task, "dns_server", configMINIMAL_STACK_SIZE * 5, 
                          NULL, 5, NULL, LED_CORE);
}
#include "common.h"
#include "http.h"
#include "esp_http_server.h"

static const char *TAG = "pixelstick-http";

bool maybe_redirect(httpd_req_t *req) {
  char* buf; 
  size_t buf_len;

  buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
  if (buf_len <= 1) {
    httpd_resp_send_404(req);
    return true;
  }

  bool has_responded = false;

  if (buf_len > 1) {
      buf = malloc(buf_len);
      if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
        if (strcmp("pixel.stick", buf) == 0) {
          // ok
        } else {
          httpd_resp_set_status(req, "307 Temporary Redirect");
          httpd_resp_set_hdr(req, "Location", "http://pixel.stick");
          httpd_resp_send(req, 
                          "Redirecting to pixel.stick", HTTPD_RESP_USE_STRLEN);
          has_responded = true;
        }
      } else {
        httpd_resp_send_404(req);
        has_responded = true;
      }
      free(buf);
  }

  return has_responded;
}

esp_err_t root_get_handler(httpd_req_t *req)
{
  if (maybe_redirect(req)) {
    return ESP_OK;
  }
  
  const char resp[] = "Hello world";
  return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

esp_err_t default_get_handler(httpd_req_t *req)
{
  if (maybe_redirect(req)) {
    return ESP_OK;
  }
  
  return httpd_resp_send_404(req);
}

/* URI handler structure for GET / */
httpd_uri_t uri_root_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = root_get_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_default_get = {
    .uri      = "/*",
    .method   = HTTP_GET,
    .handler  = default_get_handler,
    .user_ctx = NULL
};

void start_webserver() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.core_id = NET_CORE;
  config.uri_match_fn = httpd_uri_match_wildcard;
  httpd_handle_t server = NULL;

  if (httpd_start(&server, &config) == ESP_OK) {
    /* Register URI handlers */
    httpd_register_uri_handler(server, &uri_root_get);
    httpd_register_uri_handler(server, &uri_default_get);
    ESP_LOGI(TAG, "HTTP ready");
  }
}

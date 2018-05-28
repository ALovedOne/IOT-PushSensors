#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "update.h"

static const char* TAG = "UPDATE";

#define WEB_SERVER "example.com"
#define WEB_PORT 80
#define WEB_URL "http://example.com"

static const struct addrinfo hints = {
  .ai_family = AF_INET,
  .ai_socktype = SOCK_STREAM,
};

static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
	"Host: " WEB_SERVER "\r\n"
	"User-Agent: esp-idf/1.0 esp32\r\n"
	"\r\n";

static void update_app_function(void *);

void update_app_start() 
{
  TaskHandle_t xHandle = NULL;

  xTaskCreate(update_app_function, TAG, 2000, NULL, tskIDLE_PRIORITY, &xHandle);
}

static void update_app_function(void *pvParameters) 
{
  struct addrinfo *res;
  struct in_addr  *addr;

  int s;

  char recv_buf[128];
  int r;

  for ( ;; ) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Start Update");

    int err = getaddrinfo("from-ring-zero.com", "80", &hints, &res);

    if (err != 0 || res == NULL) {
      ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
      continue;
    }

    addr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
    ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));    

    s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0) {
      ESP_LOGE(TAG, "... Failed to allocate socket.");
      freeaddrinfo(res);
      continue;
    }

    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
      ESP_LOGE(TAG, "... Failed to connect errno=%d", errno);
      close(s);
      freeaddrinfo(res);
      continue;
    }

    freeaddrinfo(res);

    if (write(s, REQUEST, strlen(REQUEST)) < 0) {
      ESP_LOGE(TAG, "... socket send failed");
      close(s);
      continue;
    }
    ESP_LOGI(TAG, "... socket send success");

    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = 5;
    receiving_timeout.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
      ESP_LOGE(TAG, "... failed to set socket receiving timeout");
      close(s);
      continue;
    }
    ESP_LOGI(TAG, "... set socket receiving timeout success");

    do {
      bzero(recv_buf, sizeof(recv_buf));
      r = read(s, recv_buf, sizeof(recv_buf) - 1);
      for (int i = 0; i < r; i++) {
        putchar(recv_buf[i]);
      }
    } while (r > 0);

    ESP_LOGI(TAG, "... done reading from socket. Last read return=%d rnno=%d\r\n", r, errno);
    close(s);

    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

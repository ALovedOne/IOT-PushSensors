#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "driver/adc.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "dht.h"
#include "update.h"

#define TOKEN "yXYuUYogBmiVXSo6q76i"
#define CLIENT_ID "789c9540-54bd-11e8-ba78-6b912715438e"
#define VOTE_CNT 5

static const char *TAG = "MQTT_SAMPLE";

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;
static esp_mqtt_client_handle_t client;

static int count_votes(int , int *);
static void set_attributes(esp_mqtt_client_handle_t*);

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            set_attributes(client);

            //msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            //ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            //msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            //ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            //msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            //ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            // TODO - something
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[%s]", CONFIG_WIFI_SSID, "******");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://192.168.1.80/",
        .event_handle = mqtt_event_handler,
        .client_id = CLIENT_ID,
        .username = TOKEN,
        // .user_context = (void *)your_context
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

static void set_attributes(esp_mqtt_client_handle_t* client) {
    char buffer[128];

    snprintf(buffer, sizeof(buffer), "{\"build-time\":\"%s\"}",
                    __TIMESTAMP__);

    ESP_LOGI(TAG, "Attributes: %s", buffer);
  
    esp_mqtt_client_publish(client, "v1/devices/me/attributes", buffer, 
                    strnlen(buffer, sizeof(buffer)), 0, 0);
}

static void DHT_task(void *pvParameter)
{
  const int gpio_pin    = GPIO_NUM_23;
  const int rmt_channel = 0;

  char buffer[64];
  int msg_id;

  TickType_t pxPreviousWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 1*60*1000 / portTICK_PERIOD_MS; // 1 minutes
  //const TickType_t xFrequency = 3000 / portTICK_PERIOD_MS; // 3 seconds

  int humidity;
  int temp_x10;

  int humidity_ary[VOTE_CNT];
  int temp_x10_ary[VOTE_CNT];
  
  dht11_rmt_rx_init(gpio_pin, rmt_channel);

  sleep(2);

  for ( ;; ) {
    for (int idx = 0; idx < VOTE_CNT; ){

        vTaskDelay(3000 / portTICK_PERIOD_MS);

        if (dht11_rmt_rx(gpio_pin, rmt_channel, &humidity, &temp_x10)) {
            humidity_ary[idx] = humidity;
            temp_x10_ary[idx] = temp_x10;

            idx++;
        } else {
            ESP_LOGE(TAG, "Error");
        }

    }

    humidity = count_votes(VOTE_CNT, humidity_ary);
    temp_x10 = count_votes(VOTE_CNT, temp_x10_ary);

    ESP_LOGI(TAG, "VOTE Temp: %d  Humidity: %d", temp_x10, humidity);

    snprintf(buffer, sizeof(buffer), "{\"temperature\":%d.%d, \"humidity\":%d}", 
                    temp_x10 / 10, temp_x10 % 10, humidity);

    msg_id = esp_mqtt_client_publish(client, "v1/devices/me/telemetry", buffer, 
                    strnlen(buffer, sizeof(buffer)), 0, 0);

    // https://www.freertos.org/vtaskdelayuntil.html
    vTaskDelayUntil(&pxPreviousWakeTime, xFrequency);
  }
}

static int count_votes(int vote_count, int *votes) {
    int max_cnt = 0;
    int max_val = 0;
    for (int idx = 0; idx < vote_count; idx++) {
        int cnt = 0;
        for (int idx2 = 0; idx2 < vote_count; idx2++) {
            if (votes[idx] == votes[idx2]) {
                cnt++;
            } 
        }
        if (cnt > max_cnt) {
            max_cnt = cnt;
            max_val = votes[idx];
        }
    }
        
    return max_val; 
}

void app_main()
{
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

  ESP_ERROR_CHECK(nvs_flash_init());
  tcpip_adapter_init();
  wifi_init();
  mqtt_app_start();

  //update_app_start();

  xTaskCreate(          &DHT_task, "DHT_task", 2048, NULL, 5, NULL);
  //xTaskCreatePinnedToCore(&DHT_task, "DHT_task", 2048, NULL, 5, NULL, 0);
}

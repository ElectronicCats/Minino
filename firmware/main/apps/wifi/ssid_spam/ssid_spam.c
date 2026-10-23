#include "ssid_spam.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

static const char* TAG = "ssid_spam";
#include "esp_system.h"
#include "esp_wifi.h"
#include "wifi_service.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "string.h"

#include "animations_task.h"
#include "general_screens.h"
#include "keyboard_module.h"
#include "menus_module.h"
#include "ssid_spam_screens.h"
#include "app_manager.h"

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
static void ssid_spam_stop_task(void);
static void ssid_spam_load_stored_list(void);

/*
 * Beacon frame layout, byte by byte:
 *
 *   0..3   frame control + duration
 *   4..9   addr1 (broadcast)
 *   10..15 addr2
 *   16..21 addr3 (BSSID)
 *   22..27 sequence control + fragment number
 *   28..33 timestamp
 *   34..35 capability information
 *   36     SSID information element ID (0x00)
 *   37     SSID length          <- written per beacon
 *   38..   SSID text            <- BEACON_SSID_OFFSET
 *   then the remaining IEs
 *
 * The old beacon_raw was one flat 57-byte array holding the header AND the
 * Supported Rates / DS Param / TIM IEs, and the SSID text was appended at
 * sizeof(beacon_raw) == 57, i.e. after the TIM. beacon[37] then claimed N bytes
 * that actually belonged to those IEs, so every beacon advertised a "SSID" made
 * of raw supported-rate bytes and the intended strings were trailing garbage no
 * parser reads. The SSID region and the trailing IEs are separate now.
 */
#define BEACON_SSID_LEN_OFFSET (BEACON_SSID_OFFSET - 1)
#define BEACON_MAX_SSID_LEN 32 /* 802.11 caps the SSID at 32 octets */

/* Header through the SSID length byte. */
static const uint8_t beacon_raw[] = {
    0x80, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xba, 0xde,
    0xaf, 0xfe, 0x00, 0x06, 0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x64, 0x00, 0x31, 0x04,
    0x00, 0x00,
};

/* Supported Rates, DS Param and TIM, emitted after the SSID text. */
static const uint8_t beacon_tail[] = {
    0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 0x03, 0x01,
    0x01, 0x05, 0x04, 0x01, 0x02, 0x00, 0x00,
};

_Static_assert(BEACON_SSID_OFFSET == sizeof(beacon_raw),
               "the SSID payload must start right after the length byte");

static char* ssids_main_list[2] = {"List SSID", "Start"};
static char* ssids_list[99] = {};
static char* ssids_attack[MAX_STRINGS];
static uint16_t total_lines = 0;
static uint8_t current_item = 0;
static TaskHandle_t spam_task_handle = NULL;

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
  if (!input) {
    *count = 0;
    return;
  }
  for (uint16_t i = 0; i < *count && i < MAX_STRINGS; i++) {
    if (output_array[i]) {
      free(output_array[i]);
      output_array[i] = NULL;
    }
  }
  char* temp = strdup(input);
  if (!temp) {
    *count = 0;
    return;
  }
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
  char menu_selected[32];
  if (strlen(ssids_list[selection]) + 7 > 16) {
    snprintf(menu_selected, sizeof(menu_selected), "[%s]",
             ssids_list[selection]);
  } else {
    snprintf(menu_selected, sizeof(menu_selected), "SSID [%s]",
             ssids_list[selection]);
  }
  if (ssids_main_list[0] != NULL) {
    free(ssids_main_list[0]);
  }
  ssids_main_list[0] = strdup(menu_selected);

  storage_contex_t* item_ctx;
  item_ctx =
      flash_storage_get_item(GENFLASH_STORAGE_SPAM, ssids_list[selection]);
  if (item_ctx && item_ctx->items_storage_value) {
    split_text_into_array(item_ctx->items_storage_value, ssids_attack,
                          &total_lines);
  }
  spam_main_menu();
}

void spam_display_list_ssid() {
  storage_contex_t list[FLASH_STORAGE_MAX_LIST_ITEMS];
  /* ssids_list is also FLASH_STORAGE_MAX_LIST_ITEMS long; the count comes from
     NVS, so clamp before it indexes either array. */
  uint8_t list_count =
      flash_storage_get_list(GENFLASH_STORAGE_SPAM, list,
                             FLASH_STORAGE_MAX_LIST_ITEMS);
  for (uint8_t i = 0; i < list_count; i++) {
    ssids_list[i] = list[i].item_storage_name;
  }

  general_submenu_menu_t spam_menu_list_ssid = {0};
  spam_menu_list_ssid.options = (const char**) ssids_list;
  spam_menu_list_ssid.options_count = list_count;
  spam_menu_list_ssid.select_cb = list_ssid_cb;
  spam_menu_list_ssid.exit_cb = spam_main_menu;
  general_submenu(spam_menu_list_ssid);
}

static volatile bool spam_running = false;

static void spam_task(void* pvParameters) {
  uint16_t line = 0;
  uint8_t beacon[sizeof(beacon_raw) + BEACON_MAX_SSID_LEN +
                 sizeof(beacon_tail)];
  /* Was a VLA sized from total_lines, a runtime global loaded from NVS: the
     stack frame grew with whatever the saved SSID list happened to contain. */
  uint16_t seqnum[MAX_STRINGS];
  memset(seqnum, 0, sizeof(seqnum));

  while (spam_running) {
    vTaskDelay(pdMS_TO_TICKS(100));
    if (total_lines == 0 || line >= MAX_STRINGS ||
        ssids_attack[line] == NULL) {
      continue;
    }
    size_t ssid_len = strlen(ssids_attack[line]);
    if (ssid_len > BEACON_MAX_SSID_LEN) {
      ssid_len = BEACON_MAX_SSID_LEN;
    }

    memcpy(beacon, beacon_raw, sizeof(beacon_raw));
    memcpy(beacon + BEACON_SSID_OFFSET, ssids_attack[line], ssid_len);
    memcpy(beacon + BEACON_SSID_OFFSET + ssid_len, beacon_tail,
           sizeof(beacon_tail));
    beacon[BEACON_SSID_LEN_OFFSET] = (uint8_t) ssid_len;

    beacon[SRCADDR_OFFSET + 5] = line;
    beacon[BSSID_OFFSET + 5] = line;

    beacon[SEQNUM_OFFSET] = (seqnum[line] & 0x0f) << 4;
    beacon[SEQNUM_OFFSET + 1] = (seqnum[line] & 0xff0) >> 4;
    seqnum[line]++;
    if (seqnum[line] > 0xfff)
      seqnum[line] = 0;

    size_t frame_len = BEACON_SSID_OFFSET + ssid_len + sizeof(beacon_tail);
    esp_err_t err =
        esp_wifi_80211_tx(WIFI_IF_AP, beacon, frame_len, false);
    if (err != ESP_OK) {
      /* The return value used to be discarded, so a spam that transmitted
         nothing looked exactly like one that worked. */
      ESP_LOGW(TAG, "beacon TX failed: %s", esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (++line >= total_lines)
      line = 0;
  }

  spam_task_handle = NULL;
  vTaskDelete(NULL);
}

/* La lista de SSID se carga cuando el usuario elige un elemento del menu.
   Arrancando la app desde la consola (launch ssid_spam) no hay ningun
   elemento elegido, y el spammer se quedaba con la lista vacia: la tarea
   de spam no transmittera nada y la app parecia funcionando. Si no hay
   ninguna lista cargada, se toma el primer elemento guardado. */
static void ssid_spam_load_stored_list(void) {
  if (total_lines > 0) {
    return;
  }
  storage_contex_t list[FLASH_STORAGE_MAX_LIST_ITEMS];
  uint8_t list_count =
      flash_storage_get_list(GENFLASH_STORAGE_SPAM, list,
                             FLASH_STORAGE_MAX_LIST_ITEMS);
  for (uint8_t i = 0; i < list_count && i < FLASH_STORAGE_MAX_LIST_ITEMS; i++) {
    if (list[i].item_storage_name == NULL) {
      continue;
    }
    storage_contex_t* item = flash_storage_get_item(GENFLASH_STORAGE_SPAM,
                                                    list[i].item_storage_name);
    if (item == NULL || item->items_storage_value == NULL) {
      continue;
    }
    split_text_into_array(item->items_storage_value, ssids_attack, &total_lines);
    if (total_lines > 0) {
      ESP_LOGI(TAG, "SSID list loaded from the flash storage: %u entries (%s)",
               (unsigned) total_lines, list[i].item_storage_name);
      return;
    }
  }
  ESP_LOGW(TAG, "no SSID list in the flash storage: nothing will be transmitted");
}

static void ssid_spam_init(void) {
  ssid_spam_load_stored_list();
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  ret = esp_netif_init();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE("ssid_spam", "netif_init error: %s", esp_err_to_name(ret));
  }
  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE("ssid_spam", "event_loop_create error: %s", esp_err_to_name(ret));
  }
  /* esp_netif_create_default_wifi_ap() hace un assert() si el netif del
     AP ya existe: al reentrar en la app (o al arrancarla dos veces sin
     salir) la placa se reiniciaba con un "duplicate key". */
  if (esp_netif_get_handle_from_ifkey("WIFI_AP_DEF") == NULL) {
    esp_netif_create_default_wifi_ap();
  }

  if (!wifi_service_init(WIFI_MODE_AP)) {
    ESP_LOGE(TAG, "WiFi stack init failed");
    return;
  }

  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  wifi_config_t ap_config = {.ap = {.ssid = "NotSuspisiusCat",
                                    .ssid_len = 0,
                                    .password = "dummypassword",
                                    .channel = 1,
                                    .authmode = WIFI_AUTH_WPA2_PSK,
                                    .ssid_hidden = 1,
                                    .max_connection = 4,
                                    .beacon_interval = 60000}};

  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_set_ps(WIFI_PS_NONE);

  /* Re-entering the app used to overwrite spam_task_handle with a second
     task's handle while the first kept running: both then drove the shared
     ssids_attack[]/line and the hidden AP stayed up with no way to stop it. */
  ssid_spam_stop_task();

  spam_running = true;
  if (xTaskCreate(&spam_task, "spam_task", 4096, NULL, 5, &spam_task_handle) !=
      pdPASS) {
    ESP_LOGE(TAG, "could not create the spam task");
    spam_running = false;
    spam_task_handle = NULL;
  }
}

static void ssid_spam_stop_task(void) {
  spam_running = false;
  for (int i = 0; i < 100 && spam_task_handle != NULL; i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (spam_task_handle != NULL) {
    vTaskDelete(spam_task_handle);
    spam_task_handle = NULL;
  }
}

static void ssid_spam_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      animations_task_stop();
      ssid_spam_stop_task();
      app_manager_exit();
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
      app_manager_exit();
      break;
    case BUTTON_RIGHT:
      if (current_item == SPAM_LIST_SSID) {
        spam_display_list_ssid();
        break;
      } else if (current_item == SPAM_START) {
        if (total_lines == 0) {
          genera_screen_display_notify_information("No SSID list", "selected");
          break;
        }
        spam_start_attack();
      }
      break;
    case BUTTON_UP:
      current_item =
          (current_item == 0) ? (SPAM_COUNT - 1) : (current_item - 1);
      general_screen_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      current_item = (current_item + 1 >= SPAM_COUNT) ? 0 : (current_item + 1);
      general_screen_display_menu(current_item);
      break;
    default:
      break;
  }
}

void ssid_spam_begin() {
  /* La lista se carga al abrir la app: el menu la carga al elegir un
     elemento, pero desde la consola (launch ssid_spam) no hay ninguna
     elegida y el Start se negaba a arrancar. */
  ssid_spam_load_stored_list();
  spam_main_menu();
}


// ---------------------------------------------------------------------------
// app_manager callbacks
// ---------------------------------------------------------------------------
static void ssid_spam_on_enter(void) {
  ssid_spam_load_stored_list();
  spam_main_menu();
}

static void ssid_spam_on_input(uint8_t button_name, uint8_t button_event) {
  // Input is handled by the menu callbacks (spam_main_cb, spam_input_cb)
  // which are installed via menus_module_set_app_state
  // This callback is a fallback for when no menu is active
  (void)button_name;
  (void)button_event;
}

static void ssid_spam_on_exit(void) {
  // Stop the spam task
  animations_task_stop();
  ssid_spam_stop_task();
  // Deinitialize WiFi (started in ssid_spam_init)
  wifi_service_deinit();
  // Free attack strings
  for (uint8_t i = 0; i < MAX_STRINGS; i++) {
    if (ssids_attack[i]) {
      free(ssids_attack[i]);
      ssids_attack[i] = NULL;
    }
  }
  total_lines = 0;
  // Free main list
  if (ssids_main_list[0]) {
    free(ssids_main_list[0]);
    ssids_main_list[0] = NULL;
  }
  // Free flash storage lists
  for (uint8_t i = 0; i < FLASH_STORAGE_MAX_LIST_ITEMS; i++) {
    if (ssids_list[i]) {
      ssids_list[i] = NULL;  // These point to flash storage, don't free
    }
  }
  current_item = 0;
}

// Wrapper for menu entry
void app_manager_enter_wrapper_ssid_spam(void) {
  extern const minino_app_t ssid_spam_app;
  app_manager_enter(&ssid_spam_app);
}

// App definition for app_manager
const minino_app_t ssid_spam_app = {
  .name = "ssid_spam",
  .on_enter = ssid_spam_on_enter,
  .on_input = ssid_spam_on_input,
  .on_exit = ssid_spam_on_exit,
};


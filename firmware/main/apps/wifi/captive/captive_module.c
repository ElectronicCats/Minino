
#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "nvs_flash.h"

#include "dns_server.h"
#include "esp_http_server.h"

#include <dirent.h>
#include "files_ops.h"

#include "sd_card.h"

#include "general_notification.h"
#include "general_radio_selection.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "preferences.h"

#include "captive_module.h"

#define EXAMPLE_ESP_WIFI_SSID "MININO_CAPTIVE"
#define EXAMPLE_ESP_WIFI_PASS ""
#define EXAMPLE_MAX_STA_CONN  4

extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");

static const char* TAG = "example";

static const char* main_menu[] = {"Portals", "Mode", "Run"};
static const char* modes_menu[] = {"Standalone", "Replicate"};
static uint16_t last_index_selected = 0;
static httpd_handle_t server = NULL;

typedef enum {
  PORTALS,
  MODE,
  RUN,
} main_menu_items_t;

typedef enum { STANDALONE, REPLICATE } mode_types_t;

typedef struct {
  mode_types_t mode;
  char* portal[48];
  char* ssid[48];
  char* password[48];
} captive_context_t;

typedef struct {
  char* ent[CAPTIVE_PORTAL_LIMIT_PORTALS];
  uint16_t count;
} captive_files_t;

static captive_files_t portals_list = {0};
static captive_context_t captive_context = {0};

static void captive_module_free_portals_list(void) {
  for (int i = 0; i < portals_list.count; i++) {
    free(portals_list.ent[i]);
    portals_list.ent[i] = NULL;
  }
  portals_list.count = 0;
}

static void wifi_event_handler(void* arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void* event_data) {
  if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t* event =
        (wifi_event_ap_staconnected_t*) event_data;
    ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac),
             event->aid);
  } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t* event =
        (wifi_event_ap_stadisconnected_t*) event_data;
    ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d",
             MAC2STR(event->mac), event->aid, event->reason);
  }
}

static void wifi_init_softap(void) {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
  // &wifi_event_handler, NULL));

  wifi_config_t wifi_config = {
      .ap = {.ssid = EXAMPLE_ESP_WIFI_SSID,
             .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
             .password = EXAMPLE_ESP_WIFI_PASS,
             .max_connection = EXAMPLE_MAX_STA_CONN,
             .authmode = WIFI_AUTH_WPA_WPA2_PSK},
  };

  if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
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

  ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
           EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

static void captive_module_show_default_portal(httpd_req_t* req) {
  ESP_LOGI(TAG, "Serve root");
  const uint32_t root_len = root_end - root_start;
  httpd_resp_send(req, root_start, root_len);
}

// HTTP GET Handler
static esp_err_t root_get_handler(httpd_req_t* req) {
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

  // char* html_file = (char*) malloc(CAPTIVE_PORTAL_MAX_DEFAULT_LEN*2);
  // err = preferences_get_string(CAPTIVE_PORTAL_FS_KEY,
  // captive_context.portal;, CAPTIVE_PORTAL_MAX_DEFAULT_LEN); if (err !=
  // ESP_OK){
  //   ESP_LOGE("CAPTIVE", "ERROR reading the file name");
  //   sprintf(captive_context.portal;, CAPTIVE_PORTAL_DEFAULT_NAME);
  // }
  ESP_LOGI("CAPTIVE", "Looking the file: %s", (char*) captive_context.portal);
  // sprintf(captive_context.portal, captive_context.portal;);

  httpd_resp_set_type(req, "text/html");

  if (strcmp(captive_context.portal, CAPTIVE_PORTAL_DEFAULT_NAME) == 0) {
    captive_module_show_default_portal(req);
    // free(captive_context.portal;);
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
  ESP_LOGI("CAPTIVE", "Looking the path: %s", path);

  FILE* file = fopen(path, "r");
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return ESP_FAIL;
  }

  size_t file_size = files_ops_get_file_size_2(path);
  ESP_LOGI("CAPTIVE", "File size: %d", file_size);

  char buffer[512];
  size_t bytes_read;

  while ((bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file)) > 0) {
    buffer[bytes_read] = '\0';  // Null-terminate for httpd_resp_sendstr_chunk
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
  // free(html_file);

  if (err == ESP_OK) {
    httpd_resp_sendstr_chunk(req, NULL);
  }

  return ESP_OK;
}

static const httpd_uri_t root = {.uri = "/",
                                 .method = HTTP_GET,
                                 .handler = root_get_handler};

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t* req, httpd_err_code_t err) {
  // Set status
  httpd_resp_set_status(req, "302 Temporary Redirect");
  // Redirect to the "/" root directory
  httpd_resp_set_hdr(req, "Location", "/");
  // iOS requires content in the response to detect a captive portal, simply
  // redirecting is not sufficient.
  httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

  ESP_LOGI(TAG, "Redirecting to root");
  return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_open_sockets = 6;
  config.lru_purge_enable = true;

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &root);
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND,
                               http_404_error_handler);
  }
  return server;
}

static void captive_module_disable_wifi() {
  ESP_ERROR_CHECK(esp_wifi_stop());
  // ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
  // &wifi_event_handler));
  ESP_ERROR_CHECK(esp_wifi_deinit());
  esp_netif_t* netif = esp_netif_get_handle_from_ifkey(CAPTIVE_PORTAL_NET_NAME);
  if (netif) {
    esp_netif_destroy(netif);
    ESP_LOGI("CAPTIVE", "Netif destroyed");
  }
  if (server) {
    httpd_stop(server);
    ESP_LOGI("CAPTIVE", "HTTP server stopped");
  }

  ESP_ERROR_CHECK(esp_event_loop_delete_default());
  captive_module_main();
}

static void captive_module_wifi_begin() {
  esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
  esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
  esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
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
  ESP_LOGI("CAPTIVE", "Looking the path: %s", path);
  DIR* dir;
  dir = opendir(path);
  if (dir == NULL) {
    ESP_LOGE("CAPTIVE", "ERROR opening the SD card");
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

static void captive_module_show_running() {
  char* body[48];
  sprintf(body, "%s", (char*) captive_context.portal);

  general_notification_ctx_t notification = {0};
  notification.head = "Captive Portal";
  notification.body = body;
  notification.on_exit = captive_module_disable_wifi;
  general_notification_handler(notification);
}

static void captive_module_selector_handler(uint8_t option) {
  ESP_LOGI("CAPTIVE", "Option seletect: %s", portals_list.ent[option]);
  preferences_put_string(CAPTIVE_PORTAL_FS_KEY, portals_list.ent[option]);
  sprintf(captive_context.portal, portals_list.ent[option]);
  captive_module_main();
}

static void captive_module_sd_show_items() {
  captive_module_get_sd_items();
  if (portals_list.count < CAPTIVE_PORTAL_LIMIT_PORTALS) {
    portals_list.ent[portals_list.count] = strdup(CAPTIVE_PORTAL_DEFAULT_NAME);
    if (portals_list.ent[portals_list.count]) {
      portals_list.count++;
    } else {
      ESP_LOGE("CAPTIVE", "Failed to allocate memory for default portal");
    }
  }

  ESP_LOGW("CAPTIVE", "Count: %d", portals_list.count);
  general_submenu_menu_t portals = {0};
  portals.options = (char**) portals_list.ent;
  portals.options_count = portals_list.count;
  portals.select_cb = captive_module_selector_handler;
  portals.selected_option = last_index_selected;
  portals.exit_cb = captive_module_main;
  general_submenu(portals);
}

static void captive_module_main_menu_handler(uint8_t option) {
  switch (option) {
    case PORTALS:
      captive_module_sd_show_items();
      break;
    case MODE:
      captive_module_show_mode_selector();
      break;
    case RUN:
      captive_module_wifi_begin();
      captive_module_show_running();
      break;
    default:
      break;
  }
}

void captive_module_main(void) {
  general_submenu_menu_t main = {0};
  main.options = main_menu;
  main.options_count = sizeof(main_menu) / sizeof(char*);
  main.select_cb = captive_module_main_menu_handler;
  main.selected_option = last_index_selected;
  main.exit_cb = menus_module_restart;
  general_submenu(main);
}
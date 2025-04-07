#include "wifi_ap_manager.h"
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "preferences.h"

static const char* TAG = "ap_manager";
static EventGroupHandle_t wifi_event_group;
static app_callback callback_connection;
const int CONNECTED_BIT = BIT0;
static int reconnections = 0;

static bool wifi_ap_manager_join(const char* ssid,
                                 const char* pass,
                                 int timeout_ms);
static void initialise_wifi(void);

static void event_handler(void* arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void* event_data) {
  if (callback_connection &&
      (event_id == IP_EVENT_STA_GOT_IP || event_id == WIFI_EVENT_WIFI_READY)) {
    preferences_put_bool("wifi_connected", true);
    callback_connection(true);
  }
  if (callback_connection && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (reconnections == 0) {
      callback_connection(false);
    }
  }

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    preferences_put_bool("wifi_connected", false);
    preferences_remove("ssid");
    preferences_remove("passwd");
    if (reconnections < 3) {
      reconnections++;
      esp_wifi_connect();
    }
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    printf("Connected to AP");
    preferences_put_bool("wifi_connected", true);
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
  }
}

static void initialise_wifi(void) {
  esp_log_level_set(TAG, ESP_LOG_WARN);
  static bool initialized = false;
  if (initialized) {
    return;
  }
  ESP_ERROR_CHECK(esp_netif_init());
  wifi_event_group = xEventGroupCreate();
  esp_err_t err = esp_event_loop_create_default();
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG, "Event loop already created");
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error creating event loop: %s", esp_err_to_name(err));
    esp_restart();
  }
  esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
  assert(ap_netif);
  esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
  ESP_ERROR_CHECK(esp_wifi_start());
  initialized = true;

  // Get the IP address of the ESP32 station
  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(sta_netif, &ip_info);
  char ip_address[16];
  sprintf(ip_address, IPSTR, IP2STR(&ip_info.ip));

  ESP_LOGI(TAG, "IP Address: %s", ip_address);
}

static bool wifi_ap_manager_join(const char* ssid,
                                 const char* pass,
                                 int timeout_ms) {
  if (preferences_get_bool("wifi_connected", false)) {
    ESP_LOGI(__func__, "Already connected to AP, disconnecting...");
    esp_wifi_disconnect();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  preferences_put_bool("wifi_connected", false);
  preferences_remove("ssid");
  preferences_remove("passwd");
  initialise_wifi();
  wifi_config_t wifi_config = {0};
  strlcpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  if (pass) {
    strlcpy((char*) wifi_config.sta.password, pass,
            sizeof(wifi_config.sta.password));
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  esp_err_t err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGE(__func__, "Failed to connect to AP");
    return false;
  }

  preferences_put_string("ssid", ssid);
  preferences_put_string("passwd", pass);

  int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE,
                                 pdTRUE, timeout_ms / portTICK_PERIOD_MS);
  return (bits & CONNECTED_BIT) != 0;
}

int wifi_ap_manager_connect_index(int index) {
  char wifi_ap[100];
  char wifi_ssid[100];
  sprintf(wifi_ap, "wifi%d", index);
  esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    printf("Error getting AP");
    return 1;
  }
  char wifi_pass[100];
  err = preferences_get_string(wifi_ssid, wifi_pass, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    printf("Error getting AP");
    return 1;
  }
  return wifi_ap_manager_connect_ap(wifi_ssid, wifi_pass);
}

int wifi_ap_manager_connect_ap(const char* ssid, const char* pass) {
  ESP_LOGI(__func__, "Connecting to '%s'", ssid);
  printf("Connecting to '%s'\n", ssid);
  bool connected = wifi_ap_manager_join(ssid, pass, JOIN_TIMEOUT_MS);
  if (!connected) {
    ESP_LOGW(__func__, "Connection timed out");
    printf("Connection timed out\n");
    return 1;
  }
  return 0;
}

int wifi_ap_manager_connect_index_cb(int index, app_callback cb) {
  callback_connection = cb;
  return wifi_ap_manager_connect_index(index);
}

void wifi_ap_manager_unregister_callback() {
  callback_connection = NULL;
}

int wifi_ap_manager_delete_ap_by_index(int index) {
  char wifi_ap[100];
  char wifi_ssid[100];
  sprintf(wifi_ap, "wifi%d", index);
  esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    printf("Error getting AP");
    return 1;
  }
  char wifi_pass[100];
  err = preferences_get_string(wifi_ssid, wifi_pass, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    printf("Error getting AP");
    return 1;
  }
  err = preferences_remove(wifi_ssid);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    printf("Error getting AP");
    return 1;
  }
  err = preferences_remove(wifi_ap);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    printf("Error getting AP");
    return 1;
  }

  preferences_get_int("count_ap", 0);
  int count = preferences_get_int("count_ap", 0);
  if (count > 0) {
    count--;
  }
  preferences_put_int("count_ap", count);
  return 0;
}
#include "wifi_ap_manager.h"
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "preferences.h"
#include "radio_lease.h"
#include "wifi_service.h"

static const char* TAG = "ap_manager";
static EventGroupHandle_t wifi_event_group = NULL;
static app_callback callback_connection;
static const int CONNECTED_BIT = BIT0;
static int reconnections = 0;
static bool wifi_leased = false;

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
    reconnections = 0;
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
  }
}

static void initialise_wifi(void) {
  esp_log_level_set(TAG, ESP_LOG_WARN);
  /* One WiFi lease per session. While we hold it radio_lease cannot tear the
     driver down under us, so holding the claim implies the driver is up: never
     acquire a second time, that would leak a reference. */
  if (wifi_leased) {
    return;
  }
  if (wifi_event_group != NULL) {
    vEventGroupDelete(wifi_event_group);
    wifi_event_group = NULL;
  }
  wifi_event_group = xEventGroupCreate();
  if (wifi_event_group == NULL) {
    ESP_LOGE(TAG, "Failed to create wifi_event_group");
    return;
  }
  /* radio_lease brings up the driver (netifs, STA mode, started) and refuses
     if BLE holds the radio. */
  if (radio_lease_wifi_acquire(RADIO_WIFI_MODE_STA) != ESP_OK) {
    ESP_LOGE(TAG, "WiFi stack init failed");
    vEventGroupDelete(wifi_event_group);
    wifi_event_group = NULL;
    return;
  }
  wifi_leased = true;
  /* A connection from Settings or the console outlives that screen. As a
     background claim it gives way to the next app that needs the radio (BLE,
     or WiFi in another mode) through wifi_ap_manager_deinit(), instead of
     locking BLE out until reboot. ponytail: Modbus TCP/CatDoS sessions are
     background too, so a console BLE command run during one ends it (as
     before the lease); pin their claim if that ever matters. */
  radio_lease_wifi_set_background(wifi_ap_manager_deinit);

  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
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
  if (!wifi_leased) {
    return false; /* bring-up refused, e.g. BLE holds the radio */
  }
  wifi_config_t wifi_config = {0};
  strlcpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  if (pass) {
    strlcpy((char*) wifi_config.sta.password, pass,
            sizeof(wifi_config.sta.password));
  }

  esp_err_t err = wifi_service_set_mode(WIFI_MODE_STA);
  if (err == ESP_OK) {
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  }
  if (err == ESP_OK) {
    err = esp_wifi_connect();
  }
  if (err != ESP_OK) {
    ESP_LOGE(__func__, "Failed to connect to AP");
    return false;
  }

  preferences_put_string("ssid", ssid);
  if (pass) {
    preferences_put_string("passwd", pass);
  }

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

  int count = preferences_get_int("count_ap", 0);
  if (count > 0) {
    count--;
  }
  preferences_put_int("count_ap", count);
  return 0;
}

/* Undo initialise_wifi() and the join. Handlers go first, so the disconnect
   below does not trigger a reconnect or wipe the saved ssid/passwd, and a
   later disconnect in another app does not either. The WiFi claim is dropped
   through radio_lease, which deinitialises the driver only when we are the
   last holder: a co-running WiFi app keeps its radio, and we never stop the
   driver out from under it (the old esp_wifi_stop() did). Unregistering a
   handler that is not registered is harmless, so no guard around that. */
void wifi_ap_manager_deinit() {
  callback_connection = NULL;
  esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                               &event_handler);
  esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler);
  if (wifi_leased) {
    radio_lease_wifi_set_background(NULL);
    if (wifi_service_is_up_get()) {
      esp_wifi_disconnect(); /* leave the AP before dropping the claim */
    }
    radio_lease_wifi_release();
    wifi_leased = false;
  }
  preferences_put_bool("wifi_connected", false);
  reconnections = 0;
  if (wifi_event_group != NULL) {
    vEventGroupDelete(wifi_event_group);
    wifi_event_group = NULL;
  }
}

bool wifi_ap_manager_is_connect() {
  return preferences_get_bool("wifi_connected", false);
}

int wifi_ap_manager_get_count() {
  return preferences_get_int("count_ap", 0);
}

void wifi_ap_manager_list_aps() {
  int count = preferences_get_int("count_ap", 0);
  if (count == 0) {
    printf("No saved APs\n");
    return;
  }
  ESP_LOGI(__func__, "Saved APs: %d", count);
  for (int i = 0; i < count; i++) {
    char wifi_ap[100];
    char wifi_ssid[100];
    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      continue;
    }
    printf("[%i][%s] SSID: %s\n", i, wifi_ap, wifi_ssid);
  }
}

void wifi_ap_manager_get_aps(char** aps_list) {
  int count = wifi_ap_manager_get_count();
  if (count == 0) {
    return;
  }
  for (int i = 0; i < count; i++) {
    char wifi_ap[100];
    char wifi_ssid[100];

    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      continue;
    }
    aps_list[i] = strdup(wifi_ssid);
    printf("[%i][%s] SSID: %s\n", i, wifi_ap, wifi_ssid);
  }
}
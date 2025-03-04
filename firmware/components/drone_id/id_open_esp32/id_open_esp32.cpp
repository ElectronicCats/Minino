/* -*- tab-width: 2; mode: c; -*-
 *
 * C++ class for Arduino to function as a wrapper around opendroneid.
 * This file has the ESP32 specific code.
 *
 * Copyright (c) 2020-2023, Steve Jack.
 *
 * Nov. '22:  Split out from id_open.cpp.
 *
 * MIT licence.
 *
 * NOTES
 *
 * Bluetooth 4 works well with the opendroneid app on my G7.
 * WiFi beacon works with an ESP32 scanner, but with the G7 only the occasional
 frame gets through.
 *
 * Features
 *
 * esp_wifi_80211_tx() seems to zero the WiFi timestamp in addition to setting
 the sequence.
 * (The timestamp is set in ID_OpenDrone::transmit_wifi(), but WireShark says
 that it is zero.)
 *
 * BLE
 *
 * A case of fighting the API to get it to do what I want.
 * For certain things, it is easier to bypass the 'user friendly' Arduino API
 and
 * use the esp_ functions.
 *
 * Reference
 *
 * https://github.com/opendroneid/receiver-android/issues/7
 *
 * From the Android app -
 *
 * OpenDroneID Bluetooth beacons identify themselves by setting the GAP AD Type
 to
 * "Service Data - 16-bit UUID" and the value to 0xFFFA for ASTM International,
 ASTM Remote ID.
 * https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/
 * https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-sdos/
 * Vol 3, Part B, Section 2.5.1 of the Bluetooth 5.1 Core Specification
 * The AD Application Code is set to 0x0D = Open Drone ID.
 *
    private static final UUID SERVICE_UUID =
 UUID.fromString("0000fffa-0000-1000-8000-00805f9b34fb"); private static final
 byte[] OPEN_DRONE_ID_AD_CODE = new byte[]{(byte) 0x0D};
 *
 */

#define DIAGNOSTICS 1

//

#if defined(ARDUINO_ARCH_ESP32) || 1

  #pragma GCC diagnostic warning "-Wunused-variable"

  #include "esp_err.h"
  #include "id_open.h"

  #if ID_OD_WIFI

    #include <esp_event.h>
    #include <esp_event_loop.h>
    #include <esp_system.h>
    #include <esp_wifi.h>
    #include <esp_wifi_types.h>
    #include <nvs_flash.h>
    #include <string.h>

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx,
                            const void* buffer,
                            int len,
                            bool en_sys_seq);

  // esp_err_t event_handler(void*, system_event_t*);

    #define PASSWORD "password"
    #define SSID     "MINI Drone"
static const char* password = PASSWORD;

static bool wifi_initialized = false;

  #endif  // WIFI

  #if ID_OD_BT

    #include "BLEDevice.h"
    #include "BLEUtils.h"

static esp_ble_adv_data_t advData;
static esp_ble_adv_params_t advParams;
static BLEUUID service_uuid;

  #endif  // BT

/*
 *
 */

void construct2() {
  #if ID_OD_BT

  memset(&advData, 0, sizeof(advData));

  advData.set_scan_rsp = false;
  advData.include_name = false;
  advData.include_txpower = false;
  advData.min_interval = 0x0006;
  advData.max_interval = 0x0050;
  advData.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  memset(&advParams, 0, sizeof(advParams));

  advParams.adv_int_min = 0x0020;
  advParams.adv_int_max = 0x0040;
  advParams.adv_type = ADV_TYPE_IND;
  advParams.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  advParams.channel_map = ADV_CHNL_ALL;
  advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  advParams.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;

  service_uuid = BLEUUID("0000fffa-0000-1000-8000-00805f9b34fb");

  #endif  // ID_OD_BT

  return;
}

/*
 *
 */

void init2(char* ssid,
           int ssid_length,
           uint8_t* WiFi_mac_addr,
           uint8_t wifi_channel) {
  int status;
  char text[128];

  status = 0;
  text[0] = text[63] = 0;

  #if ID_OD_WIFI
  if (!wifi_initialized) {
    wifi_initialized = true;
  } else {
    return;
  }

  // Inicializa NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Inicializa la pila TCP/IP y el manejador de eventos
  ESP_ERROR_CHECK(esp_netif_init());
  esp_event_loop_create_default();

  // Crea una interfaz de red Wi-Fi en modo AP
  esp_netif_create_default_wifi_ap();

  // Configuración por defecto de Wi-Fi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Registra el manejador de eventos para eventos Wi-Fi
  // ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
  // ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

  // Configuración del punto de acceso
  wifi_config_t wifi_manager_config = {0};
  strcpy((char*) wifi_manager_config.ap.ssid, ssid);
  strcpy((char*) wifi_manager_config.ap.password, password);
  wifi_manager_config.ap.ssid_len = strlen(ssid);
  wifi_manager_config.ap.channel = (uint8_t) wifi_channel;
  wifi_manager_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_manager_config.ap.ssid_hidden = 0;
  wifi_manager_config.ap.max_connection = 4;
  wifi_manager_config.ap.beacon_interval = 1000;
  // Si no se establece una contraseña, el modo de autenticación se configura
  // como abierto
  if (strlen(PASSWORD) == 0) {
    wifi_manager_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  // Establece el modo Wi-Fi en AP y aplica la configuración
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_manager_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  // Establece la potencia de transmisión máxima
  int8_t wifi_power =
      78;  // Potencia máxima en unidades de 0.25 dBm (78 * 0.25 = 19.5 dBm)
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(wifi_power));

  // Establece el ancho de banda del canal
  ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20));

  // Desactiva el modo de ahorro de energía
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    // no default mac addresses
    // status = esp_read_mac(WiFi_mac_addr,ESP_MAC_WIFI_STA);

    //   if (Debug_Serial) {

    //     sprintf(text,"esp_read_mac():  %02x:%02x:%02x:%02x:%02x:%02x\r\n",
    //             WiFi_mac_addr[0],WiFi_mac_addr[1],WiFi_mac_addr[2],
    //             WiFi_mac_addr[3],WiFi_mac_addr[4],WiFi_mac_addr[5]);
    //     Debug_Serial->print(text);
    // // power <= 72, dbm = power/4, but 78 = 20dbm.
    //     sprintf(text,"max_tx_power():  %d dBm\r\n",(int) ((wifi_power + 2) /
    //     4)); Debug_Serial->print(text); sprintf(text,"wifi country:
    //     %s\r\n",country.cc); Debug_Serial->print(text);
    //   }

  #endif  // WIFI

  #if ID_OD_BT

  int power_db;
  esp_power_level_t power;

  BLEDevice::init(ssid);

  // Using BLEDevice::setPower() seems to have no effect.
  // ESP_PWR_LVL_N12 ...  ESP_PWR_LVL_N0 ... ESP_PWR_LVL_P9

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

  power = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
  power_db = 3 * ((int) power - 4);

  #endif

  return;
}

/*
 * Processor dependent bits for the wifi frame header.
 */

uint8_t* capability() {
  // 0x21 = ESS | Short preamble
  // 0x04 = Short slot time

  static uint8_t capa[2] = {0x21, 0x04};

  return capa;
}

//

int tag_rates(uint8_t* beacon_frame, int beacon_offset) {
  beacon_frame[beacon_offset++] = 0x01;
  beacon_frame[beacon_offset++] = 0x08;
  beacon_frame[beacon_offset++] = 0x8b;  //  5.5
  beacon_frame[beacon_offset++] = 0x96;  // 11
  beacon_frame[beacon_offset++] = 0x82;  //  1
  beacon_frame[beacon_offset++] = 0x84;  //  2
  beacon_frame[beacon_offset++] = 0x0c;  //  6
  beacon_frame[beacon_offset++] = 0x18;  // 12
  beacon_frame[beacon_offset++] = 0x30;  // 24
  beacon_frame[beacon_offset++] = 0x60;  // 48

  return beacon_offset;
}

//

int tag_ext_rates(uint8_t* beacon_frame, int beacon_offset) {
  beacon_frame[beacon_offset++] = 0x32;
  beacon_frame[beacon_offset++] = 0x04;
  beacon_frame[beacon_offset++] = 0x6c;  // 54
  beacon_frame[beacon_offset++] = 0x12;  //  9
  beacon_frame[beacon_offset++] = 0x24;  // 18
  beacon_frame[beacon_offset++] = 0x48;  // 36

  return beacon_offset;
}

//

int misc_tags(uint8_t* beacon_frame, int beacon_offset) {
  return beacon_offset;
}

/*
 *
 */

int transmit_wifi2(uint8_t* buffer, int length) {
  esp_err_t wifi_status = 0;

  #if ID_OD_WIFI

  if (length) {
    wifi_status = esp_wifi_80211_tx(WIFI_IF_AP, buffer, length, true);
  }

  #endif

  return (int) wifi_status;
}

/*
 *
 */

int transmit_ble2(uint8_t* ble_message, int length) {
  esp_err_t ble_status = 0;

  #if ID_OD_BT

  static int advertising = 0;

  if (advertising) {
    ble_status = esp_ble_gap_stop_advertising();
  }

  ble_status = esp_ble_gap_config_adv_data_raw(ble_message, length);
  ble_status = esp_ble_gap_start_advertising(&advParams);

  advertising = 1;

  #endif  // BT

  return (int) ble_status;
}

/*
 *
 */

  #if ID_OD_WIFI

  // esp_err_t event_handler(void* ctx, system_event_t* event) {
  //   return ESP_OK;
  // }

  #endif

/*
 *
 */

#endif

#include <string.h>
#include "droneid_scanner_screens.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "odid_wifi.h"

static const char* TAG = "DroneidScanner";

static ODID_UAS_Data UAS_data;

static void store_mac(uav_data* uav, uint8_t* payload) {
  memcpy(uav->mac, &payload[10], 6);
}

static void parse_odid(uav_data* UAV, ODID_UAS_Data* UAS_data2) {
  memset(UAV->op_id, 0, sizeof(UAV->op_id));
  memset(UAV->uav_id, 0, sizeof(UAV->uav_id));
  memset(UAV->description, 0, sizeof(UAV->description));
  memset(UAV->auth_data, 0, sizeof(UAV->auth_data));

  if (UAS_data2->BasicIDValid[0]) {
    strncpy(UAV->uav_id, (char*) UAS_data2->BasicID[0].UASID, ODID_ID_SIZE);
  }
  if (UAS_data2->LocationValid) {
    UAV->lat_d = UAS_data2->Location.Latitude;
    UAV->long_d = UAS_data2->Location.Longitude;
    UAV->altitude_msl = (int) UAS_data2->Location.AltitudeGeo;
    UAV->height_agl = (int) UAS_data2->Location.Height;
    UAV->speed = (int) UAS_data2->Location.SpeedHorizontal;
    UAV->heading = (int) UAS_data2->Location.Direction;
    UAV->speed_vertical = (int) UAS_data2->Location.SpeedVertical;
    UAV->altitude_pressure = (int) UAS_data2->Location.AltitudeBaro;
    UAV->height_type = UAS_data2->Location.HeightType;
    UAV->horizontal_accuracy = UAS_data2->Location.HorizAccuracy;
    UAV->vertical_accuracy = UAS_data2->Location.VertAccuracy;
    UAV->baro_accuracy = UAS_data2->Location.BaroAccuracy;
    UAV->speed_accuracy = UAS_data2->Location.SpeedAccuracy;
    UAV->timestamp = (int) UAS_data2->Location.TimeStamp;
    UAV->status = UAS_data2->Location.Status;
  }
  if (UAS_data2->SystemValid) {
    UAV->base_lat_d = UAS_data2->System.OperatorLatitude;
    UAV->base_long_d = UAS_data2->System.OperatorLongitude;
    UAV->operator_location_type = UAS_data2->System.OperatorLocationType;
    UAV->classification_type = UAS_data2->System.ClassificationType;
    UAV->area_count = UAS_data2->System.AreaCount;
    UAV->area_radius = UAS_data2->System.AreaRadius;
    UAV->area_ceiling = UAS_data2->System.AreaCeiling;
    UAV->area_floor = UAS_data2->System.AreaFloor;
    UAV->operator_altitude_geo = UAS_data2->System.OperatorAltitudeGeo;
    UAV->system_timestamp = UAS_data2->System.Timestamp;
  }
  if (UAS_data2->AuthValid[0]) {
    UAV->auth_type = UAS_data2->Auth[0].AuthType;
    UAV->auth_page = UAS_data2->Auth[0].DataPage;
    UAV->auth_length = UAS_data2->Auth[0].Length;
    UAV->auth_timestamp = UAS_data2->Auth[0].Timestamp;
    memcpy(UAV->auth_data, UAS_data2->Auth[0].AuthData,
           sizeof(UAV->auth_data) - 1);
  }
  if (UAS_data2->SelfIDValid) {
    UAV->desc_type = UAS_data2->SelfID.DescType;
    strncpy(UAV->description, UAS_data2->SelfID.Desc, ODID_STR_SIZE);
  }
  if (UAS_data2->OperatorIDValid) {
    UAV->operator_id_type = UAS_data2->OperatorID.OperatorIdType;
    strncpy(UAV->op_id, (char*) UAS_data2->OperatorID.OperatorId, ODID_ID_SIZE);
  }
}

static void callback(uint8_t* buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*) buf;
  // Get the packet header
  uint8_t* payload = p->payload;
  int packet_len = p->rx_ctrl.sig_len;
  uint8_t pkt_type = buf[12];
  uav_data* currentUAV = (uav_data*) malloc(sizeof(uav_data));

  if (!currentUAV)
    return;

  memset(currentUAV, 0, sizeof(uav_data));

  store_mac(currentUAV, payload);

  /**
   * Packet subtype
   * 0x00 = Management
   * 0x80 = Beacon
   * 0x13 = Action
   */
  uint8_t astm_1std[3] = {0x50, 0x6f, 0x9a};
  uint8_t astm_2std[3] = {0x90, 0x3a, 0xe6};
  uint8_t astm_3std[3] = {0xfa, 0x0b, 0xbc};
  uint8_t dji_1[3] = {0x60, 0x60, 0x1F};
  uint8_t dji_2[3] = {0x48, 0x1C, 0xB9};
  uint8_t dji_3[3] = {0x34, 0xD2, 0x62};

  static const uint8_t nan_dest[6] = {0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00};
  if (memcmp(nan_dest, &payload[4], 6) == 0) {
    // ESP_LOGI(TAG, "NAN packet detected");
    if (odid_wifi_receive_message_pack_nan_action_frame(
            &UAS_data, (char*) currentUAV->op_id, payload, packet_len) == 0) {
      parse_odid(currentUAV, &UAS_data);
      droneid_scanner_update_list(currentUAV->mac, currentUAV);
    }
  } else {
    int offset = BEACON_OFFSET;
    bool parsed = false;
    while (offset < packet_len) {
      int payload_type = payload[offset];
      int payload_len = payload[offset + 1];
      if (!parsed) {
        if ((payload_type == 0xdd) &&
            ((memcmp(&payload[offset + 2], astm_1std, sizeof(astm_1std)) ==
              0) ||
             (memcmp(&payload[offset + 2], astm_2std, sizeof(astm_2std)) ==
              0) ||
             (memcmp(&payload[offset + 2], astm_3std, sizeof(astm_3std)) ==
              0) ||
             (memcmp(&payload[offset + 2], dji_1, sizeof(dji_1)) == 0) ||
             (memcmp(&payload[offset + 2], dji_2, sizeof(dji_2)) == 0) ||
             (memcmp(&payload[offset + 2], dji_3, sizeof(dji_3)) == 0))) {
          // ESP_LOGI(TAG, "ODID packet detected");
          int idx = offset + BEACON_PACKET_OFFSET;
          if (idx < packet_len) {
            memset(&UAS_data, 0, sizeof(UAS_data));
            odid_message_process_pack(&UAS_data, &payload[idx],
                                      packet_len - idx);
            parse_odid(currentUAV, &UAS_data);
            droneid_scanner_update_list(currentUAV->mac, currentUAV);
            parsed = true;
          }
        }
      }
      offset += payload_len + 2;
    }
  }

  free(currentUAV);
}

static int droneid_scanner_init_wifi(void) {
  esp_err_t err = esp_netif_init();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing wifi: %s", esp_err_to_name(err));
    return err;
  }
  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error setting storage: %s", esp_err_to_name(err));
    return err;
  }
  err = esp_wifi_set_mode(WIFI_MODE_NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error setting mode: %s", esp_err_to_name(err));
    return err;
  }
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(callback);

  err = esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error setting channel: %s", esp_err_to_name(err));
    return err;
  }

  err = esp_wifi_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error starting wifi: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "Droneid scanner started");
  return ESP_OK;
}

void droneid_scanner_begin() {
  esp_err_t err = droneid_scanner_init_wifi();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error starting droneid scanner");
    return;
  }

  droneid_scanner_screen_main();
}
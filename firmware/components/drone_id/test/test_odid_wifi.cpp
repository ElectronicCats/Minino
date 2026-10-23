// SPDX-License-Identifier: Apache-2.0
#include <string.h>
#include <errno.h>
#include "odid_wifi.h"
#include "drone_id_test.h"

static void setup_sample_uas_data(ODID_UAS_Data* uasData) {
  odid_initUasData(uasData);

  uasData->BasicIDValid[0] = 1;
  uasData->BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
  uasData->BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
  strncpy(uasData->BasicID[0].UASID, "DRONE-12345", sizeof(uasData->BasicID[0].UASID));

  uasData->LocationValid = 1;
  uasData->Location.Status = ODID_STATUS_AIRBORNE;
  uasData->Location.Latitude = 37.7749;
  uasData->Location.Longitude = -122.4194;
  uasData->Location.AltitudeGeo = 100.0f;
  uasData->Location.SpeedHorizontal = 10.0f;
}

TEST_CASE("odid_wifi_build_nan_sync_beacon_frame builds valid beacon", "[odid_wifi]") {
  char mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t buf[512];

  // NULL safety
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_wifi_build_nan_sync_beacon_frame(NULL, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_wifi_build_nan_sync_beacon_frame(mac, NULL, sizeof(buf)));

  // Buffer too small
  TEST_ASSERT_EQUAL_INT(-ENOMEM, odid_wifi_build_nan_sync_beacon_frame(mac, buf, 10));

  // Valid build
  int len = odid_wifi_build_nan_sync_beacon_frame(mac, buf, sizeof(buf));
  TEST_ASSERT_TRUE(len > 0);
}

TEST_CASE("odid_wifi_build_message_pack_beacon_frame builds beacon and prevents overflow", "[odid_wifi]") {
  ODID_UAS_Data uasData;
  setup_sample_uas_data(&uasData);

  char mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t buf[512];
  const char* ssid = "Drone-WiFi-Beacon";

  // Invalid SSID inputs
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_wifi_build_message_pack_beacon_frame(&uasData, mac, NULL, 0, 100, 1, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_wifi_build_message_pack_beacon_frame(&uasData, mac, ssid, 0, 100, 1, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_wifi_build_message_pack_beacon_frame(&uasData, mac, ssid, 33, 100, 1, buf, sizeof(buf)));

  // Buffer too small for SSID (Verifies bug fix: previous check only tested sizeof(*ssid_s) and then overflowed memcpy!)
  // Header + beacon takes 36 bytes. If buf_size is 38, it holds header + 2 bytes of ssid_s, but NOT the 17 bytes of SSID!
  TEST_ASSERT_EQUAL_INT(-ENOMEM, odid_wifi_build_message_pack_beacon_frame(&uasData, mac, ssid, strlen(ssid), 100, 1, buf, 38));

  // Valid build with sufficient buffer
  int len = odid_wifi_build_message_pack_beacon_frame(&uasData, mac, ssid, strlen(ssid), 100, 1, buf, sizeof(buf));
  TEST_ASSERT_TRUE(len > 0);
}

TEST_CASE("odid_wifi_build_message_pack_nan_action_frame builds action frame", "[odid_wifi]") {
  ODID_UAS_Data uasData;
  setup_sample_uas_data(&uasData);

  char mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t buf[512];

  // NULL safety
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_wifi_build_message_pack_nan_action_frame(NULL, mac, 1, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_wifi_build_message_pack_nan_action_frame(&uasData, NULL, 1, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_wifi_build_message_pack_nan_action_frame(&uasData, mac, 1, NULL, sizeof(buf)));

  // Valid build
  int len = odid_wifi_build_message_pack_nan_action_frame(&uasData, mac, 1, buf, sizeof(buf));
  TEST_ASSERT_TRUE(len > 0);
}

TEST_CASE("odid_message_process_pack validates bounds and corrupted size", "[odid_wifi]") {
  ODID_UAS_Data uasData;
  setup_sample_uas_data(&uasData);

  uint8_t packBuf[256];
  int packLen = odid_message_build_pack(&uasData, packBuf, sizeof(packBuf));
  TEST_ASSERT_TRUE(packLen > 0);

  // NULL and short buflen checks
  ODID_UAS_Data decData;
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_message_process_pack(NULL, packBuf, packLen));
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_message_process_pack(&decData, NULL, packLen));
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_message_process_pack(&decData, packBuf, 2));

  // Corrupted MsgPackSize exceeding maximum
  uint8_t corruptedPack[256];
  memcpy(corruptedPack, packBuf, packLen);
  ODID_MessagePack_encoded* enc = (ODID_MessagePack_encoded*) corruptedPack;
  enc->MsgPackSize = 50; // exceeds ODID_PACK_MAX_MESSAGES (9)
  TEST_ASSERT_EQUAL_INT(-EINVAL, odid_message_process_pack(&decData, corruptedPack, sizeof(corruptedPack)));

  // Valid pack processing
  int procLen = odid_message_process_pack(&decData, packBuf, packLen);
  TEST_ASSERT_EQUAL_INT(packLen, procLen);
  TEST_ASSERT_EQUAL_INT(1, decData.BasicIDValid[0]);
  TEST_ASSERT_EQUAL_STRING("DRONE-12345", decData.BasicID[0].UASID);
}

TEST_CASE("drone_export_gps_data handles NULL, zero buffer, and LastPageIndex overflow", "[odid_wifi]") {
  ODID_UAS_Data uasData;
  setup_sample_uas_data(&uasData);

  char jsonBuf[1024];

  // NULL and zero size checks
  drone_export_gps_data(NULL, jsonBuf, sizeof(jsonBuf));
  drone_export_gps_data(&uasData, NULL, sizeof(jsonBuf));
  drone_export_gps_data(&uasData, jsonBuf, 0);

  // Valid export
  drone_export_gps_data(&uasData, jsonBuf, sizeof(jsonBuf));
  TEST_ASSERT_NOT_NULL(strstr(jsonBuf, "DRONE-12345"));
  TEST_ASSERT_NOT_NULL(strstr(jsonBuf, "Location"));

  // LastPageIndex boundary check (Bug fix: untrusted LastPageIndex must not access out-of-bounds Auth pages!)
  uasData.Auth[0].LastPageIndex = 255;
  drone_export_gps_data(&uasData, jsonBuf, sizeof(jsonBuf));
  TEST_ASSERT_TRUE(strlen(jsonBuf) > 0);
}

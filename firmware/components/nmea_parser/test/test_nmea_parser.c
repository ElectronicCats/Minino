// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nmea_parser.h"
#include "nmea_test.h"

static gps_t s_last_gps;
static bool s_update_called = false;
static bool s_unknown_called = false;
static char s_last_unknown[64];

static void test_gps_event_handler(void* event_handler_arg,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void* event_data) {
  (void) event_handler_arg;
  (void) event_base;
  if (event_id == GPS_UPDATE) {
    s_update_called = true;
    if (event_data) {
      memcpy(&s_last_gps, event_data, sizeof(gps_t));
    }
  } else if (event_id == GPS_UNKNOWN) {
    s_unknown_called = true;
    if (event_data) {
      strncpy(s_last_unknown, (const char*) event_data, sizeof(s_last_unknown) - 1);
      s_last_unknown[sizeof(s_last_unknown) - 1] = '\0';
    }
  }
}

static nmea_parser_handle_t create_test_parser(void) {
  nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
  nmea_parser_handle_t hdl = nmea_parser_init(&config);
  s_update_called = false;
  s_unknown_called = false;
  memset(&s_last_gps, 0, sizeof(s_last_gps));
  memset(s_last_unknown, 0, sizeof(s_last_unknown));
  nmea_parser_add_handler(hdl, test_gps_event_handler, NULL);
  return hdl;
}

TEST_CASE("init rejects NULL configuration", "[nmea]") {
  nmea_parser_handle_t hdl = nmea_parser_init(NULL);
  TEST_ASSERT_NULL(hdl);
}

TEST_CASE("deinit rejects NULL handle", "[nmea]") {
  esp_err_t err = nmea_parser_deinit(NULL);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("add_handler and remove_handler reject NULL arguments", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, nmea_parser_add_handler(NULL, test_gps_event_handler, NULL));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, nmea_parser_add_handler(hdl, NULL, NULL));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, nmea_parser_remove_handler(NULL, test_gps_event_handler));

  nmea_parser_deinit(hdl);
}

TEST_CASE("decode rejects NULL or zero length arguments", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, nmea_parser_decode(NULL, "$GPGGA\r\n", 8));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, nmea_parser_decode(hdl, NULL, 8));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, nmea_parser_decode(hdl, "$GPGGA\r\n", 0));

  nmea_parser_deinit(hdl);
}

TEST_CASE("get_gps_data returns valid pointer or NULL on invalid handle", "[nmea]") {
  TEST_ASSERT_NULL(nmea_parser_get_gps_data(NULL));
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);
  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_NOT_NULL(gps);
  nmea_parser_deinit(hdl);
}

TEST_CASE("parse GGA statement correctly", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // $GPGGA,123519.50,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*42
  const char* gga = "$GPGGA,123519.50,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*42\r\n";
  esp_err_t err = nmea_parser_decode(hdl, gga, strlen(gga));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_NOT_NULL(gps);

  TEST_ASSERT_EQUAL_UINT8(12, gps->tim.hour);
  TEST_ASSERT_EQUAL_UINT8(35, gps->tim.minute);
  TEST_ASSERT_EQUAL_UINT8(19, gps->tim.second);
  TEST_ASSERT_EQUAL_UINT16(500, gps->tim.thousand);

  // 48 deg + 07.038 / 60 = 48.1173
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 48.1173f, gps->latitude);
  // 11 deg + 31.000 / 60 = 11.51667
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.5167f, gps->longitude);

  TEST_ASSERT_EQUAL(GPS_FIX_GPS, gps->fix);
  TEST_ASSERT_EQUAL_UINT8(8, gps->sats_in_use);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9f, gps->dop_h);
  // 545.4 + 46.9 = 592.3
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 592.3f, gps->altitude);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse South and West coordinates correctly in GGA", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // South latitude and West longitude:
  // $GPGGA,000000,3351.000,S,15112.000,W,1,04,1.5,10.0,M,0.0,M,,*52
  const char* gga_sw = "$GPGGA,000000,3351.000,S,15112.000,W,1,04,1.5,10.0,M,0.0,M,,*52\r\n";
  esp_err_t err = nmea_parser_decode(hdl, gga_sw, strlen(gga_sw));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_TRUE(gps->latitude < 0);
  TEST_ASSERT_TRUE(gps->longitude < 0);
  // 33 + 51/60 = 33.85 -> -33.85
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -33.85f, gps->latitude);
  // 151 + 12/60 = 151.2 -> -151.2
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -151.2f, gps->longitude);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse RMC statement with date, speed, cog, and validity", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // $GPRMC,083559,A,4717.11437,N,00833.91522,E,0.004,77.52,091204,003.1,W*43
  const char* rmc = "$GPRMC,083559,A,4717.11437,N,00833.91522,E,0.004,77.52,091204,003.1,W*43\r\n";
  esp_err_t err = nmea_parser_decode(hdl, rmc, strlen(rmc));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_TRUE(gps->valid);
  TEST_ASSERT_EQUAL_UINT8(8, gps->tim.hour);
  TEST_ASSERT_EQUAL_UINT8(35, gps->tim.minute);
  TEST_ASSERT_EQUAL_UINT8(59, gps->tim.second);
  TEST_ASSERT_EQUAL_UINT16(0, gps->tim.thousand);

  // Date: 09/12/04 -> day 9, month 12, year 4
  TEST_ASSERT_EQUAL_UINT8(9, gps->date.day);
  TEST_ASSERT_EQUAL_UINT8(12, gps->date.month);
  TEST_ASSERT_EQUAL_UINT16(4, gps->date.year);

  // Speed: 0.004 knots * 1.852 = 0.007408 m/s
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.007408f, gps->speed);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 77.52f, gps->cog);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.1f, gps->variation);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse RMC with truncated date string without crash", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // Date field is empty ,,
  // $GPRMC,123519,V,,,,,,,23,*74
  const char* rmc_short = "$GPRMC,123519,V,,,,,,,23,*74\r\n";
  esp_err_t err = nmea_parser_decode(hdl, rmc_short, strlen(rmc_short));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_FALSE(gps->valid);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse GSA statement for fix mode, dop, and satellite IDs", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // $GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39
  const char* gsa = "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39\r\n";
  esp_err_t err = nmea_parser_decode(hdl, gsa, strlen(gsa));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_EQUAL(GPS_MODE_3D, gps->fix_mode);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, gps->dop_p);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.3f, gps->dop_h);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.1f, gps->dop_v);

  TEST_ASSERT_EQUAL_UINT8(4, gps->sats_id_in_use[0]);
  TEST_ASSERT_EQUAL_UINT8(5, gps->sats_id_in_use[1]);
  TEST_ASSERT_EQUAL_UINT8(0, gps->sats_id_in_use[2]);
  TEST_ASSERT_EQUAL_UINT8(9, gps->sats_id_in_use[3]);
  TEST_ASSERT_EQUAL_UINT8(12, gps->sats_id_in_use[4]);
  TEST_ASSERT_EQUAL_UINT8(24, gps->sats_id_in_use[7]);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse GSV statement with multiple satellites in view", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // $GPGSV,2,1,08,01,40,083,46,02,17,308,41,12,07,344,39,14,66,039,40*7E
  const char* gsv1 = "$GPGSV,2,1,08,01,40,083,46,02,17,308,41,12,07,344,39,14,66,039,40*7E\r\n";
  esp_err_t err = nmea_parser_decode(hdl, gsv1, strlen(gsv1));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_EQUAL_UINT8(8, gps->sats_in_view);

  // Sat 0: prn=1, el=40, az=83, snr=46
  TEST_ASSERT_EQUAL_UINT8(1, gps->sats_desc_in_view[0].num);
  TEST_ASSERT_EQUAL_UINT8(40, gps->sats_desc_in_view[0].elevation);
  TEST_ASSERT_EQUAL_UINT16(83, gps->sats_desc_in_view[0].azimuth);
  TEST_ASSERT_EQUAL_UINT8(46, gps->sats_desc_in_view[0].snr);

  // Sat 1: prn=2, el=17, az=308, snr=41
  TEST_ASSERT_EQUAL_UINT8(2, gps->sats_desc_in_view[1].num);
  TEST_ASSERT_EQUAL_UINT8(17, gps->sats_desc_in_view[1].elevation);
  TEST_ASSERT_EQUAL_UINT16(308, gps->sats_desc_in_view[1].azimuth);
  TEST_ASSERT_EQUAL_UINT8(41, gps->sats_desc_in_view[1].snr);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse GSV with sat_num zero does not underflow or crash", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // Corrupted sat_num = 0
  // $GPGSV,2,0,08,01,40,083,46*7F
  const char* bad_gsv = "$GPGSV,2,0,08,01,40,083,46*7F\r\n";
  esp_err_t err = nmea_parser_decode(hdl, bad_gsv, strlen(bad_gsv));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse GLL statement correctly", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // $GPGLL,3723.2475,N,12158.3416,W,161229.48,A*2C
  const char* gll = "$GPGLL,3723.2475,N,12158.3416,W,161229.48,A*2C\r\n";
  esp_err_t err = nmea_parser_decode(hdl, gll, strlen(gll));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_TRUE(gps->valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 37.3874f, gps->latitude);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -121.9723f, gps->longitude);
  TEST_ASSERT_EQUAL_UINT8(16, gps->tim.hour);
  TEST_ASSERT_EQUAL_UINT8(12, gps->tim.minute);
  TEST_ASSERT_EQUAL_UINT8(29, gps->tim.second);
  TEST_ASSERT_EQUAL_UINT16(480, gps->tim.thousand);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse VTG statement correctly", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // $GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48
  const char* vtg = "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48\r\n";
  esp_err_t err = nmea_parser_decode(hdl, vtg, strlen(vtg));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 54.7f, gps->cog);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 34.4f, gps->variation);
  // 10.2 km/h / 3.6 = 2.833 m/s
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.833f, gps->speed);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse ZDA statement parses day, month, 4-digit year separately", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // $GPZDA,143025.00,15,08,2024,00,00*6B
  // Field 1: 143025.00 (time)
  // Field 2: 15 (day)
  // Field 3: 08 (month)
  // Field 4: 2024 (year)
  const char* zda = "$GPZDA,143025.00,15,08,2024,00,00*6B\r\n";
  esp_err_t err = nmea_parser_decode(hdl, zda, strlen(zda));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_EQUAL_UINT8(14, gps->tim.hour);
  TEST_ASSERT_EQUAL_UINT8(30, gps->tim.minute);
  TEST_ASSERT_EQUAL_UINT8(25, gps->tim.second);
  TEST_ASSERT_EQUAL_UINT16(0, gps->tim.thousand);

  // Verify that day, month, and year are parsed separately and accurately (Bug fix verified)
  TEST_ASSERT_EQUAL_UINT8(15, gps->date.day);
  TEST_ASSERT_EQUAL_UINT8(8, gps->date.month);
  TEST_ASSERT_EQUAL_UINT16(2024, gps->date.year);

  nmea_parser_deinit(hdl);
}

TEST_CASE("reject statement with corrupted checksum", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // Correct CRC for $GPVTG,054.7,T,034.4,M,005.5,N,010.2,K is 48. Send 99:
  const char* bad_crc = "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*99\r\n";
  nmea_parser_decode(hdl, bad_crc, strlen(bad_crc));

  // Update should NOT be called on CRC error
  TEST_ASSERT_FALSE(s_update_called);

  nmea_parser_deinit(hdl);
}

TEST_CASE("reject statement without asterisk or truncated", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  const char* truncated = "$GPGGA,123519,4807.038,N\r\n";
  nmea_parser_decode(hdl, truncated, strlen(truncated));

  TEST_ASSERT_FALSE(s_update_called);

  nmea_parser_deinit(hdl);
}

TEST_CASE("unknown statement with valid CRC triggers GPS_UNKNOWN event", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // $PTEST,1,2,3*5A: XOR of PTEST,1,2,3 is 0x5A
  const char* unknown = "$PTEST,1,2,3*5A\r\n";
  nmea_parser_decode(hdl, unknown, strlen(unknown));

  TEST_ASSERT_TRUE(s_unknown_called);

  nmea_parser_deinit(hdl);
}

TEST_CASE("unknown statement with bad CRC does not trigger GPS_UNKNOWN event", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  const char* bad_unknown = "$PTEST,1,2,3*00\r\n";
  nmea_parser_decode(hdl, bad_unknown, strlen(bad_unknown));

  TEST_ASSERT_FALSE(s_unknown_called);

  nmea_parser_deinit(hdl);
}

TEST_CASE("item longer than 16 characters is safely truncated without overflow", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  // Huge string of digits in altitude field (item 9):
  // "012345678901234567890123456789"
  const char* long_item = "$GPGGA,000000,,,,,0,00,,12345678901234567890,M,,M,,*52\r\n";
  esp_err_t err = nmea_parser_decode(hdl, long_item, strlen(long_item));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  nmea_parser_deinit(hdl);
}

TEST_CASE("parse multiple statements in a single stream", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  const char* multi =
      "$GPGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";

  esp_err_t err = nmea_parser_decode(hdl, multi, strlen(multi));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_TRUE(gps->valid);
  TEST_ASSERT_EQUAL(GPS_FIX_GPS, gps->fix);
  TEST_ASSERT_EQUAL_UINT8(23, gps->date.day);
  TEST_ASSERT_EQUAL_UINT8(3, gps->date.month);
  TEST_ASSERT_EQUAL_UINT16(94, gps->date.year);

  nmea_parser_deinit(hdl);
}

TEST_CASE("handle noise and garbage before statement", "[nmea]") {
  nmea_parser_handle_t hdl = create_test_parser();
  TEST_ASSERT_NOT_NULL(hdl);

  const char* noisy =
      "garbage\xFF\xFE\xAA\x55~!!\r\n"
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";

  esp_err_t err = nmea_parser_decode(hdl, noisy, strlen(noisy));
  TEST_ASSERT_EQUAL(ESP_OK, err);

  const gps_t* gps = nmea_parser_get_gps_data(hdl);
  TEST_ASSERT_TRUE(gps->valid);

  nmea_parser_deinit(hdl);
}

// SPDX-License-Identifier: Apache-2.0
#include <string.h>
#include <cmath>
#include "utm.h"
#include "drone_id_test.h"

TEST_CASE("calc_m_per_deg handles NULL pointers safely", "[utm]") {
  UTM_Utilities utils;
  double m_lat = 0, m_lon = 0;

  // Passing NULL pointers should not crash
  utils.calc_m_per_deg(45.0, NULL, &m_lon);
  utils.calc_m_per_deg(45.0, &m_lat, NULL);
  utils.calc_m_per_deg(45.0, NULL, NULL);

  TEST_ASSERT_EQUAL_DOUBLE(0.0, m_lat);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, m_lon);
}

TEST_CASE("calc_m_per_deg computes reasonable meters per degree at equator and poles", "[utm]") {
  UTM_Utilities utils;
  double m_lat = 0, m_lon = 0;

  // At Equator (lat 0): 1 deg lon is approx 111.3 km, 1 deg lat is approx 110.5 km
  utils.calc_m_per_deg(0.0, &m_lat, &m_lon);
  TEST_ASSERT_DOUBLE_WITHIN(1000.0, 110574.0, m_lat);
  TEST_ASSERT_DOUBLE_WITHIN(1000.0, 111319.0, m_lon);

  // At 45 degrees
  utils.calc_m_per_deg(45.0, &m_lat, &m_lon);
  TEST_ASSERT_DOUBLE_WITHIN(1000.0, 111132.0, m_lat);
  TEST_ASSERT_DOUBLE_WITHIN(1000.0, 78847.0, m_lon);

  // At North Pole (lat 90): lon meters per degree is zero
  utils.calc_m_per_deg(90.0, &m_lat, &m_lon);
  TEST_ASSERT_DOUBLE_WITHIN(1e-4, 0.0, m_lon);
}

TEST_CASE("luhn36 conversion functions c2i and i2c", "[utm]") {
  UTM_Utilities utils;

  TEST_ASSERT_EQUAL_INT(0, utils.luhn36_c2i('0'));
  TEST_ASSERT_EQUAL_INT(9, utils.luhn36_c2i('9'));
  TEST_ASSERT_EQUAL_INT(10, utils.luhn36_c2i('a'));
  TEST_ASSERT_EQUAL_INT(35, utils.luhn36_c2i('z'));
  TEST_ASSERT_EQUAL_INT(10, utils.luhn36_c2i('A'));
  TEST_ASSERT_EQUAL_INT(35, utils.luhn36_c2i('Z'));
  TEST_ASSERT_EQUAL_INT(0, utils.luhn36_c2i('!'));

  TEST_ASSERT_EQUAL_CHAR('0', utils.luhn36_i2c(0));
  TEST_ASSERT_EQUAL_CHAR('9', utils.luhn36_i2c(9));
  TEST_ASSERT_EQUAL_CHAR('a', utils.luhn36_i2c(10));
  TEST_ASSERT_EQUAL_CHAR('z', utils.luhn36_i2c(35));
  TEST_ASSERT_EQUAL_CHAR('0', utils.luhn36_i2c(36));
}

TEST_CASE("luhn36_check calculation and NULL safety", "[utm]") {
  UTM_Utilities utils;

  // NULL safety
  TEST_ASSERT_EQUAL_CHAR('0', utils.luhn36_check(NULL));

  // Compute check char for known test string
  char check = utils.luhn36_check("1234567890ab");
  TEST_ASSERT_TRUE((check >= '0' && check <= '9') || (check >= 'a' && check <= 'z'));
}

TEST_CASE("check_EU_op_id verifies valid EU Operator ID", "[utm]") {
  UTM_Utilities utils;

  // Construct valid ID: 3-letter country code (e.g. "FIN") + 12 alphanumeric + 1 check character
  // Let's compute the check character for payload "123456789012" + secret "xyz":
  char test_s[16];
  strcpy(test_s, "123456789012xyz");
  char expected_check = utils.luhn36_check(test_s);

  char valid_id[17];
  snprintf(valid_id, sizeof(valid_id), "FIN123456789012%c", expected_check);

  TEST_ASSERT_EQUAL_INT(1, utils.check_EU_op_id(valid_id, "xyz"));

  // Wrong secret
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id(valid_id, "abc"));

  // Wrong checksum digit in ID
  valid_id[15] = (expected_check == 'a') ? 'b' : 'a';
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id(valid_id, "xyz"));
}

TEST_CASE("check_EU_op_id rejects NULL and invalid length inputs without buffer overflow", "[utm]") {
  UTM_Utilities utils;

  // NULL pointers
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id(NULL, "xyz"));
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id("FIN123456789012a", NULL));
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id(NULL, NULL));

  // Short ID with valid secret (Verifies bug fix for De Morgan operator bug where short ID was accepted!)
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id("FIN", "xyz"));
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id("", "xyz"));
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id("12345", "xyz"));

  // Long ID
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id("FIN1234567890123456789", "xyz"));

  // Wrong length secret
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id("FIN123456789012a", "xy"));
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id("FIN123456789012a", "xyza"));
  TEST_ASSERT_EQUAL_INT(0, utils.check_EU_op_id("FIN123456789012a", ""));
}

// SPDX-License-Identifier: Apache-2.0
#include "alt_unix_time.h"
#include "drone_id_test.h"

TEST_CASE("julian_day calculates known astronomical dates", "[alt_unix_time]") {
  // Julian Day for 1970-01-01 00:00:00 is 2440587.5
  double jd_1970 = julian_day(1970, 1, 1, 1);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 2440587.5, jd_1970);

  // Sputnik date: 1957-10-04.81 -> approx 2436116.31
  double jd_sputnik = julian_day(1957, 10, 4.81f, 1);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 2436116.31, jd_sputnik);

  // Julian calendar date: 333-01-27.5
  double jd_333 = julian_day(333, 1, 27.5f, 0);
  TEST_ASSERT_DOUBLE_WITHIN(0.05, 1842713.0, jd_333);
}

TEST_CASE("alt_unix_secs calculates Unix timestamps accurately",
          "[alt_unix_time]") {
  // Epoch: 1970-01-01 00:00:00 -> 0
  uint64_t epoch = alt_unix_secs(1970, 1, 1, 0, 0, 0);
  TEST_ASSERT_EQUAL_UINT64(0, epoch);

  // 1970-01-01 01:00:00 -> 3600
  uint64_t one_hour = alt_unix_secs(1970, 1, 1, 1, 0, 0);
  TEST_ASSERT_EQUAL_UINT64(3600, one_hour);

  // 2024-01-01 00:00:00 UTC -> 1704067200
  uint64_t y2024 = alt_unix_secs(2024, 1, 1, 0, 0, 0);
  TEST_ASSERT_EQUAL_UINT64(1704067200, y2024);
}

TEST_CASE("alt_unix_secs protects against underflow for pre-1970 dates",
          "[alt_unix_time]") {
  // Year before 1970 returns 0 and does not underflow to a huge uint64_t
  uint64_t pre_epoch = alt_unix_secs(1969, 12, 31, 23, 59, 59);
  TEST_ASSERT_EQUAL_UINT64(0, pre_epoch);

  uint64_t way_back = alt_unix_secs(1900, 1, 1, 0, 0, 0);
  TEST_ASSERT_EQUAL_UINT64(0, way_back);
}

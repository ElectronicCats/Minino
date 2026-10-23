// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include "nmea_test.h"

#define NMEA_TEST_MAX 256

static struct {
  const char* name;
  const char* tags;
  nmea_test_fn fn;
} s_t[NMEA_TEST_MAX];
static int s_n;

void nmea_test_register(const char* name, const char* tags, nmea_test_fn fn) {
  if (s_n < NMEA_TEST_MAX) {
    s_t[s_n].name = name;
    s_t[s_n].tags = tags;
    s_t[s_n].fn = fn;
    s_n++;
  }
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
  UNITY_BEGIN();
  for (int i = 0; i < s_n; i++) {
    UnityDefaultTestRun(s_t[i].fn, s_t[i].name, 0);
  }
  return UNITY_END();
}

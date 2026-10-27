// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include <stdio.h>
#include "drone_id_test.h"

#define DRONE_TEST_MAX 256

static struct {
  const char* name;
  const char* tags;
  drone_test_fn fn;
} s_t[DRONE_TEST_MAX];
static int s_n;

extern "C" void drone_test_register(const char* name,
                                    const char* tags,
                                    drone_test_fn fn) {
  if (s_n < DRONE_TEST_MAX) {
    s_t[s_n].name = name;
    s_t[s_n].tags = tags;
    s_t[s_n].fn = fn;
    s_n++;
  }
}

// Stubs for hardware/radio specific callbacks referenced by id_open.cpp
void construct2(void) {}
void init_w(char*, int, uint8_t*, uint8_t) {}
void init_ble() {}
int transmit_wifi2(uint8_t*, int) {
  return 0;
}
int transmit_ble2(uint8_t*, int) {
  return 0;
}
uint8_t* capability(void) {
  static uint8_t cap[2] = {0, 0};
  return cap;
}
int tag_rates(uint8_t*, int offset) {
  return offset;
}
int tag_ext_rates(uint8_t*, int offset) {
  return offset;
}
int misc_tags(uint8_t*, int offset) {
  return offset;
}
void set_wifi_ap(char*, uint8_t) {}

extern "C" {
void setUp(void) {}
void tearDown(void) {}

int main(void) {
  UNITY_BEGIN();
  for (int i = 0; i < s_n; i++) {
    UnityDefaultTestRun(s_t[i].fn, s_t[i].name, 0);
  }
  return UNITY_END();
}
}

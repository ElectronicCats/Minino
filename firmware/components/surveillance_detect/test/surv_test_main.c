// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include "surv_test.h"

#define SURV_TEST_MAX 256

static struct {
  const char* name;
  const char* tags;
  surv_test_fn fn;
} s_t[SURV_TEST_MAX];
static int s_n;

void surv_test_register(const char* name, const char* tags, surv_test_fn fn) {
  if (s_n < SURV_TEST_MAX) {
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

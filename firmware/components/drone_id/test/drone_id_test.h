// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "unity.h"

typedef void (*drone_test_fn)(void);
void drone_test_register(const char* name, const char* tags, drone_test_fn fn);

#define DRONE_CAT_(a, b) a##b
#define DRONE_CAT(a, b)  DRONE_CAT_(a, b)

// Registers test function via constructor attribute before main().
#define TEST_CASE(desc, tags)                                          \
  static void DRONE_CAT(drone_test_, __LINE__)(void);                  \
  __attribute__((constructor)) static void DRONE_CAT(drone_reg_,       \
                                                     __LINE__)(void) { \
    drone_test_register(desc, tags, DRONE_CAT(drone_test_, __LINE__)); \
  }                                                                    \
  static void DRONE_CAT(drone_test_, __LINE__)(void)

#ifdef __cplusplus
}
#endif

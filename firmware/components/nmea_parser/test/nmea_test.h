// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "unity.h"

typedef void (*nmea_test_fn)(void);
void nmea_test_register(const char* name, const char* tags, nmea_test_fn fn);

#define NMEA_CAT_(a, b) a##b
#define NMEA_CAT(a, b)  NMEA_CAT_(a, b)

// Registers test function via constructor attribute before main().
#define TEST_CASE(desc, tags)                                         \
  static void NMEA_CAT(nmea_test_, __LINE__)(void);                   \
  __attribute__((constructor)) static void NMEA_CAT(nmea_reg_,        \
                                                    __LINE__)(void) { \
    nmea_test_register(desc, tags, NMEA_CAT(nmea_test_, __LINE__));   \
  }                                                                   \
  static void NMEA_CAT(nmea_test_, __LINE__)(void)

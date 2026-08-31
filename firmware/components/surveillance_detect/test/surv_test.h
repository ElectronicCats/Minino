// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "unity.h"

typedef void (*surv_test_fn)(void);
void surv_test_register(const char* name, const char* tags, surv_test_fn fn);

#define SURV_CAT_(a, b) a##b
#define SURV_CAT(a, b)  SURV_CAT_(a, b)

// Declara el test y un constructor que lo registra antes de main().
#define TEST_CASE(desc, tags)                                         \
  static void SURV_CAT(surv_test_, __LINE__)(void);                   \
  __attribute__((constructor)) static void SURV_CAT(surv_reg_,        \
                                                    __LINE__)(void) { \
    surv_test_register(desc, tags, SURV_CAT(surv_test_, __LINE__));   \
  }                                                                   \
  static void SURV_CAT(surv_test_, __LINE__)(void)

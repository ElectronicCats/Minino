// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_signatures.h"
#include "surv_test.h"
#include "surv_types.h"

TEST_CASE("la tabla base tiene 75 OUIs", "[surv][signatures]") {
  TEST_ASSERT_EQUAL_UINT16(75, surv_signatures_oui_count());
}

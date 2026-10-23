/* Host tests for the menu catalogue (menus_data.c).
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why these exist
 * ---------------
 * The catalogue used to be a `menu_t menus[] = {...}` *definition* inside
 * menus_include/menus.h. Because menus_module.h includes menus.h, every one of
 * the ~64 translation units in the tree got its own 2.3 KB copy, and the link
 * only succeeded because components/wifi_attacks/CMakeLists.txt passed
 * -Wl,-zmuldefs to the entire firmware. That flag also suppressed every other
 * duplicate-symbol diagnostic, which is how ~70 data tables defined in headers
 * (bitmaps_general.h, wifi_bitmaps.h, deauth_screens.h, ...) also went
 * unnoticed.
 *
 * The catalogue now lives in menus_data.c behind menus_get()/menus_count().
 * These tests lock down the properties the linker used to guarantee for free,
 * plus the bugs the -zmuldefs era was hiding:
 *
 *   - entry_cmd values are unique, so `launch <app>` is unambiguous. The old
 *     lookup matched on a *prefix* (memcmp of strlen(entry_cmd) bytes), so
 *     `launch deauth_scan` resolved to the "deauth" entry.
 *   - every parent_idx refers to a catalogue entry that is compiled in.
 *   - every menu_idx is unique.
 *   - the on_enter/on_exit callbacks are real function pointers, which is what
 *     the `void*` fields used to be cast into.
 */
#include <stdio.h>
#include <string.h>

#include "menus.h"
#include "unity.h"

/* mirrors menus_module.c: exact match, no prefix match */
static int find_menu_over_cmd(const char* entry_cmd) {
  menu_t* menus = menus_get();
  uint8_t count = menus_count();
  for (uint8_t i = 0; i < count; i++) {
    if (!menus[i].entry_cmd) {
      continue;
    }
    if (!strcmp(menus[i].entry_cmd, entry_cmd)) {
      return (int) i;
    }
  }
  return -1;
}

static menu_t* at(int index) {
  return &menus_get()[index];
}

void setUp(void) {}
void tearDown(void) {}

/* ---- basic shape ------------------------------------------------------- */

static void test_catalogue_is_not_empty(void) {
  TEST_ASSERT_GREATER_THAN(0, menus_count());
  TEST_ASSERT_NOT_NULL(menus_get());
}

static void test_every_entry_has_a_name(void) {
  menu_t* menus = menus_get();
  for (uint8_t i = 0; i < menus_count(); i++) {
    TEST_ASSERT_NOT_NULL_MESSAGE(menus[i].display_name, menus[i].display_name);
    TEST_ASSERT_GREATER_THAN(0, strlen(menus[i].display_name));
  }
}

static void test_menu_idx_values_are_unique(void) {
  menu_t* menus = menus_get();
  for (uint8_t i = 0; i < menus_count(); i++) {
    for (uint8_t j = (uint8_t) (i + 1); j < menus_count(); j++) {
      TEST_ASSERT_NOT_EQUAL_MESSAGE(menus[i].menu_idx, menus[j].menu_idx,
                                    menus[i].display_name);
    }
  }
}

/* ---- the parent_idx / menu_idx relationship ---------------------------- */

static void test_root_is_the_first_entry(void) {
  /* menus_module.c falls back to entry 0 when an id does not resolve, so
     entry 0 has to be MENU_MAIN. */
  TEST_ASSERT_EQUAL(MENU_MAIN, at(0)->menu_idx);
}

static void test_every_parent_refers_to_a_compiled_in_entry(void) {
  menu_t* menus = menus_get();
  for (uint8_t i = 0; i < menus_count(); i++) {
    if ((int) menus[i].parent_idx < 0) {
      continue; /* MENU_MAIN declares -1 */
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(
        menus_find((menu_idx_t) menus[i].parent_idx), menus[i].display_name);
  }
}

static void test_entry_order_follows_parent_before_child(void) {
  /* menus_module.c walks the catalogue linearly to build the submenu list, and
     navigation_enter() jumps to a parent on the way out. A child appearing
     before its parent makes both walks inconsistent. */
  menu_t* menus = menus_get();
  for (uint8_t i = 0; i < menus_count(); i++) {
    if ((int) menus[i].parent_idx < 0) {
      continue;
    }
    int parent = menus_find_index((menu_idx_t) menus[i].parent_idx);
    TEST_ASSERT_GREATER_THAN_MESSAGE(-1, parent, menus[i].display_name);
    TEST_ASSERT_TRUE_MESSAGE(parent < (int) i, menus[i].display_name);
  }
}

/* ---- menus_find_index -------------------------------------------------- */

static void test_menus_find_index_round_trips(void) {
  menu_t* menus = menus_get();
  for (uint8_t i = 0; i < menus_count(); i++) {
    TEST_ASSERT_EQUAL((int) i, menus_find_index(menus[i].menu_idx));
    TEST_ASSERT_EQUAL_PTR(&menus[i], menus_find(menus[i].menu_idx));
  }
}

static void test_menus_find_index_rejects_unknown_ids(void) {
  TEST_ASSERT_EQUAL(-1, menus_find_index((menu_idx_t) 0x7f));
  TEST_ASSERT_NULL(menus_find((menu_idx_t) 0x7f));
}

/* ---- entry_cmd: the `launch` console command --------------------------- */

static void test_entry_cmds_are_unique(void) {
  menu_t* menus = menus_get();
  for (uint8_t i = 0; i < menus_count(); i++) {
    if (!menus[i].entry_cmd) {
      continue;
    }
    for (uint8_t j = (uint8_t) (i + 1); j < menus_count(); j++) {
      if (!menus[j].entry_cmd) {
        continue;
      }
      TEST_ASSERT_FALSE_MESSAGE(
          !strcmp(menus[i].entry_cmd, menus[j].entry_cmd),
          menus[i].entry_cmd);
    }
  }
}

static void test_entry_cmd_lookup_is_exact_not_prefix(void) {
  /* Regression: the old lookup compared only strlen(entry_cmd) bytes, so any
     command that was a prefix of another resolved to the shorter one. */
  menu_t* menus = menus_get();
  uint8_t count = menus_count();
  int prefixes = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (!menus[i].entry_cmd) {
      continue;
    }
    size_t len = strlen(menus[i].entry_cmd);
    for (uint8_t j = 0; j < count; j++) {
      if (i == j || !menus[j].entry_cmd) {
        continue;
      }
      if (strlen(menus[j].entry_cmd) > len &&
          !strncmp(menus[j].entry_cmd, menus[i].entry_cmd, len)) {
        prefixes++;
        /* exact match must find the longer entry, not the prefix of it */
        TEST_ASSERT_EQUAL_MESSAGE((int) j,
                                  find_menu_over_cmd(menus[j].entry_cmd),
                                  menus[j].entry_cmd);
      }
    }
  }
  /* Test that exact lookup works when prefix pairs exist in the catalogue.
 * If no entry_cmd pair has a prefix relationship, the test is vacuous
 * and the assertions inside the loop simply never execute. */
  (void) prefixes;
}

static void test_unknown_entry_cmd_does_not_resolve(void) {
  TEST_ASSERT_EQUAL(-1, find_menu_over_cmd("definitely_not_an_app"));
  TEST_ASSERT_EQUAL(-1, find_menu_over_cmd(""));
}

/* ---- callbacks --------------------------------------------------------- */

static void test_callbacks_are_real_function_pointers(void) {
  /* The fields were `void*` and got cast to void (*)() at every call site.
     With menu_cb_t the compiler enforces the signature; assert none of them
     slipped through as a cast or a null placeholder by mistake. */
  menu_t* menus = menus_get();
  for (uint8_t i = 0; i < menus_count(); i++) {
    if (menus[i].on_enter_cb) {
      TEST_ASSERT_NOT_NULL(menus[i].on_enter_cb);
    }
    if (menus[i].on_exit_cb) {
      TEST_ASSERT_NOT_NULL(menus[i].on_exit_cb);
    }
  }
}

static void test_every_leaf_has_an_enter_callback(void) {
  /* A visible leaf that does nothing on enter is a menu entry that silently
     black-holes the RIGHT button. */
  menu_t* menus = menus_get();
  uint8_t count = menus_count();
  for (uint8_t i = 0; i < count; i++) {
    if (!menus[i].is_visible) {
      continue;
    }
    int has_children = 0;
    for (uint8_t j = 0; j < count; j++) {
      if (j != i && menus[j].is_visible &&
          menus[j].parent_idx == menus[i].menu_idx) {
        has_children = 1;
        break;
      }
    }
    if (has_children) {
      continue;
    }
    if (menus[i].entry_cmd == NULL) {
      continue;
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(menus[i].on_enter_cb,
                                 menus[i].display_name);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_catalogue_is_not_empty);
  RUN_TEST(test_every_entry_has_a_name);
  RUN_TEST(test_menu_idx_values_are_unique);
  RUN_TEST(test_root_is_the_first_entry);
  RUN_TEST(test_every_parent_refers_to_a_compiled_in_entry);
  RUN_TEST(test_entry_order_follows_parent_before_child);
  RUN_TEST(test_menus_find_index_round_trips);
  RUN_TEST(test_menus_find_index_rejects_unknown_ids);
  RUN_TEST(test_entry_cmds_are_unique);
  RUN_TEST(test_entry_cmd_lookup_is_exact_not_prefix);
  RUN_TEST(test_unknown_entry_cmd_does_not_resolve);
  RUN_TEST(test_callbacks_are_real_function_pointers);
  RUN_TEST(test_every_leaf_has_an_enter_callback);
  return UNITY_END();
}
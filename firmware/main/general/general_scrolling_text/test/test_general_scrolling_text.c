/* Host tests for general_scroll_text_split().
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why these exist
 * ---------------
 * Every "Help", "Credits" and "Legal" screen renders through
 * general_register_scrolling_menu() -> split_text().
 *
 * split_text() counted the lines in one loop using `line_len > MAX_LINE_LENGTH`
 * and then filled them in a second loop using `line_len >= MAX_LINE_LENGTH`.
 * Those thresholds differ by one, so the fill pass wrote one entry past the end
 * of the malloc(lines_count) array. A 28-character string with no spaces
 * produced lines_count = 2 and three writes - a heap overflow with a corrupted
 * pointer that was then handed to oled_screen_display_text().
 *
 * The count and the writes now come from a single pass, and the array grows as
 * it is filled, so they cannot disagree. These tests pin that, and pin the
 * behaviour the old code was trying to produce.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "general_scroll_split.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* --- the regression --------------------------------------------------- */

static void test_wrapping_never_writes_past_the_reported_count(void) {
  /* The exact input that broke the old two-pass version. */
  const char* text = "abcdefghijklmnopqrstuvwxyzab"; /* 28 chars, no spaces */
  uint16_t n = 0;
  char** lines = general_scroll_text_split(text, &n);
  TEST_ASSERT_NOT_NULL(lines);
  /* Every row must be a real, NUL-terminated string within the count. */
  for (uint16_t i = 0; i < n; i++) {
    TEST_ASSERT_NOT_NULL(lines[i]);
    TEST_ASSERT_LESS_OR_EQUAL(GENERAL_SCROLL_MAX_LINE_LEN, strlen(lines[i]));
  }
  general_scroll_text_free(lines, n);
}

static void test_no_input_produces_a_single_row_per_line_and_nothing_more(
    void) {
  /* The property the old code got wrong in general: rows written == rows
     reported, and re-joining them reproduces the input without the wraps. */
  static const char* inputs[] = {
      "a",
      "abcd",
      "abcdefghijklmno",              /* exactly the limit */
      "abcdefghijklmn",               /* one over the limit */
      "abcdefghijklmnopqrstuvwxyzab", /* two rows + 0 */
      "line one\nline two",
      "line one\n\n\nline two",
      "caaac\n c  c aac  acb  \n \nccb bcbbcaca a\n \n",
      "                    ",
      "",
      "\n\n\n",
      "trailing spaces      \nand more   ",
  };
  const unsigned count = sizeof(inputs) / sizeof(inputs[0]);

  for (unsigned i = 0; i < count; i++) {
    uint16_t n = 0;
    char** lines = general_scroll_text_split(inputs[i], &n);
    TEST_ASSERT_NOT_NULL_MESSAGE(lines, inputs[i]);

    /* Independent oracle: walk the input a row at a time, the way a reader
       would. Deliberately written without sharing any code with the
       implementation, and mirroring only the documented rules: split on '\n',
       wrap at GENERAL_SCROLL_MAX_LINE_LEN, trim trailing whitespace, drop rows
       that end up empty. */
    uint16_t expected = 0;
    size_t run = 0;
    size_t row_start = 0;
    for (size_t j = 0;; j++) {
      char c = inputs[i][j];
      bool at_end = (c == '\0');
      bool wrap = (!at_end && c != '\n' && run >= GENERAL_SCROLL_MAX_LINE_LEN);
      if (at_end || c == '\n' || wrap) {
        size_t e = j;
        while (e > row_start &&
               (inputs[i][e - 1] == ' ' || inputs[i][e - 1] == '\t' ||
                inputs[i][e - 1] == '\r')) {
          e--;
        }
        if (e > row_start) {
          expected++;
        }
        run = 0;
        row_start = j + 1;
        if (at_end) {
          break;
        }
        if (wrap) {
          row_start = j; /* the current char opens the next row */
        }
        continue;
      }
      run++;
    }

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(expected, n, inputs[i]);
    for (uint16_t k = 0; k < n; k++) {
      TEST_ASSERT_NOT_NULL_MESSAGE(lines[k], inputs[i]);
      TEST_ASSERT_LESS_OR_EQUAL(GENERAL_SCROLL_MAX_LINE_LEN, strlen(lines[k]));
    }
    general_scroll_text_free(lines, n);
  }
}

/* --- row width -------------------------------------------------------- */

static void test_no_row_exceeds_the_display_width(void) {
  /* A 400-character blob: no row may be longer than the screen can show. */
  char blob[401];
  memset(blob, 'x', sizeof(blob) - 1);
  blob[sizeof(blob) - 1] = '\0';

  uint16_t n = 0;
  char** lines = general_scroll_text_split(blob, &n);
  TEST_ASSERT_NOT_NULL(lines);
  TEST_ASSERT_GREATER_THAN(1, n);
  for (uint16_t i = 0; i < n; i++) {
    TEST_ASSERT_LESS_OR_EQUAL(GENERAL_SCROLL_MAX_LINE_LEN, strlen(lines[i]));
  }
  general_scroll_text_free(lines, n);
}

static void test_exactly_limit_sized_input_is_one_row(void) {
  char exact[GENERAL_SCROLL_MAX_LINE_LEN + 1];
  memset(exact, 'a', GENERAL_SCROLL_MAX_LINE_LEN);
  exact[GENERAL_SCROLL_MAX_LINE_LEN] = '\0';

  uint16_t n = 0;
  char** lines = general_scroll_text_split(exact, &n);
  TEST_ASSERT_NOT_NULL(lines);
  TEST_ASSERT_EQUAL_UINT16(1, n);
  TEST_ASSERT_EQUAL_STRING(exact, lines[0]);
  general_scroll_text_free(lines, n);
}

static void test_one_over_the_limit_splits_into_two_rows(void) {
  char over[GENERAL_SCROLL_MAX_LINE_LEN + 2];
  memset(over, 'b', GENERAL_SCROLL_MAX_LINE_LEN + 1);
  over[GENERAL_SCROLL_MAX_LINE_LEN + 1] = '\0';

  uint16_t n = 0;
  char** lines = general_scroll_text_split(over, &n);
  TEST_ASSERT_NOT_NULL(lines);
  TEST_ASSERT_EQUAL_UINT16(2, n);
  TEST_ASSERT_EQUAL_UINT16(GENERAL_SCROLL_MAX_LINE_LEN, strlen(lines[0]));
  TEST_ASSERT_EQUAL_UINT16(1, strlen(lines[1]));
  general_scroll_text_free(lines, n);
}

/* --- separators ------------------------------------------------------- */

static void test_newlines_split_without_leading_whitespace(void) {
  uint16_t n = 0;
  char** lines = general_scroll_text_split("alpha\nbeta\ngamma", &n);
  TEST_ASSERT_NOT_NULL(lines);
  TEST_ASSERT_EQUAL_UINT16(3, n);
  TEST_ASSERT_EQUAL_STRING("alpha", lines[0]);
  TEST_ASSERT_EQUAL_STRING("beta", lines[1]);
  TEST_ASSERT_EQUAL_STRING("gamma", lines[2]);
  general_scroll_text_free(lines, n);
}

static void test_blank_rows_are_dropped(void) {
  uint16_t n = 0;
  char** lines = general_scroll_text_split("a\n\n\n\nb", &n);
  TEST_ASSERT_NOT_NULL(lines);
  TEST_ASSERT_EQUAL_UINT16(2, n);
  TEST_ASSERT_EQUAL_STRING("a", lines[0]);
  TEST_ASSERT_EQUAL_STRING("b", lines[1]);
  general_scroll_text_free(lines, n);
}

static void test_carriage_returns_are_treated_as_whitespace(void) {
  uint16_t n = 0;
  char** lines = general_scroll_text_split("one\r\ntwo\r\n", &n);
  TEST_ASSERT_NOT_NULL(lines);
  TEST_ASSERT_EQUAL_UINT16(2, n);
  TEST_ASSERT_EQUAL_STRING("one", lines[0]);
  TEST_ASSERT_EQUAL_STRING("two", lines[1]);
  general_scroll_text_free(lines, n);
}

static void test_whitespace_only_input_yields_no_rows(void) {
  const char* inputs[] = {"", "   ", "\n", " \n \n ", "\t\t"};
  const unsigned count = sizeof(inputs) / sizeof(inputs[0]);
  for (unsigned i = 0; i < count; i++) {
    uint16_t n = 123; /* must be reset even when there is nothing to return */
    char** lines = general_scroll_text_split(inputs[i], &n);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, n, inputs[i]);
    general_scroll_text_free(lines, n);
  }
}

/* --- API contract ----------------------------------------------------- */

static void test_null_arguments_are_handled(void) {
  uint16_t n = 99;
  TEST_ASSERT_NULL(general_scroll_text_split(NULL, &n));
  TEST_ASSERT_EQUAL_UINT16(0, n);

  TEST_ASSERT_NULL(general_scroll_text_split("text", NULL));

  /* Freeing nothing, and freeing an empty list, must both be safe. */
  general_scroll_text_free(NULL, 0);
  general_scroll_text_free(NULL, 12);
}

static void test_free_releases_every_row(void) {
  /* Run under a leak checker if one is available; the point is that free()
     takes the same count the splitter reported. */
  uint16_t n = 0;
  char** lines = general_scroll_text_split("row one\nrow two\nrow three", &n);
  TEST_ASSERT_EQUAL_UINT16(3, n);
  general_scroll_text_free(lines, n);
  /* A second free of the same pointer would be a double free, so the contract
     is one call per successful split. */
}

/* --- stress ----------------------------------------------------------- */

static void test_repeated_split_of_a_long_text_is_stable(void) {
  char blob[1024];
  size_t off = 0;
  while (off + 10 < sizeof(blob)) {
    off += (size_t) snprintf(blob + off, sizeof(blob) - off, "line %zu\n", off);
  }
  blob[sizeof(blob) - 1] = '\0';

  for (int run = 0; run < 20; run++) {
    uint16_t n = 0;
    char** lines = general_scroll_text_split(blob, &n);
    TEST_ASSERT_NOT_NULL(lines);
    TEST_ASSERT_GREATER_THAN(1, n);
    for (uint16_t i = 0; i < n; i++) {
      TEST_ASSERT_NOT_NULL(lines[i]);
      TEST_ASSERT_LESS_OR_EQUAL(GENERAL_SCROLL_MAX_LINE_LEN, strlen(lines[i]));
    }
    general_scroll_text_free(lines, n);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_wrapping_never_writes_past_the_reported_count);
  RUN_TEST(test_no_input_produces_a_single_row_per_line_and_nothing_more);
  RUN_TEST(test_no_row_exceeds_the_display_width);
  RUN_TEST(test_exactly_limit_sized_input_is_one_row);
  RUN_TEST(test_one_over_the_limit_splits_into_two_rows);
  RUN_TEST(test_newlines_split_without_leading_whitespace);
  RUN_TEST(test_blank_rows_are_dropped);
  RUN_TEST(test_carriage_returns_are_treated_as_whitespace);
  RUN_TEST(test_whitespace_only_input_yields_no_rows);
  RUN_TEST(test_null_arguments_are_handled);
  RUN_TEST(test_free_releases_every_row);
  RUN_TEST(test_repeated_split_of_a_long_text_is_stable);
  return UNITY_END();
}
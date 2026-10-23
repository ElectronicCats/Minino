/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "general_scroll_split.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void general_scroll_text_free(char** lines, uint16_t num_lines) {
  if (lines == NULL) {
    return;
  }
  for (uint16_t i = 0; i < num_lines; i++) {
    free(lines[i]);
  }
  free(lines);
}

/*
 * Single pass, with the array grown as needed.
 *
 * The previous implementation counted the lines in one loop with
 * `line_len > MAX_LINE_LENGTH` and then filled them in a second loop with
 * `line_len >= MAX_LINE_LENGTH`. Those two thresholds differ by one, so the
 * fill pass wrote one entry past the end of the malloc'ed array - a heap
 * overflow reachable from every screen that uses
 * general_register_scrolling_menu(). Building the result as we go makes the
 * count and the writes impossible to disagree.
 */
char** general_scroll_text_split(const char* text, uint16_t* num_lines) {
  if (num_lines != NULL) {
    *num_lines = 0;
  }
  if (text == NULL || num_lines == NULL) {
    return NULL;
  }

  const size_t len = strlen(text);
  size_t capacity = 8;
  size_t count = 0;
  char** lines = (char**) malloc(capacity * sizeof(char*));
  if (!lines) {
    return NULL;
  }

/* Appends text[begin, end) as a row, trimming trailing whitespace and
   skipping rows that end up empty. */
#define EMIT_ROW(begin, end)                                          \
  do {                                                                \
    size_t _b = (begin);                                              \
    size_t _e = (end);                                                \
    while (_e > _b && (text[_e - 1] == ' ' || text[_e - 1] == '\t' || \
                       text[_e - 1] == '\r')) {                       \
      _e--;                                                           \
    }                                                                 \
    if (_e > _b) {                                                    \
      if (count == capacity) {                                        \
        char** _bigger =                                              \
            (char**) realloc(lines, capacity * 2 * sizeof(char*));    \
        if (!_bigger) {                                               \
          general_scroll_text_free(lines, (uint16_t) count);          \
          return NULL;                                                \
        }                                                             \
        lines = _bigger;                                              \
        capacity *= 2;                                                \
      }                                                               \
      char* _line = (char*) malloc(_e - _b + 1);                      \
      if (!_line) {                                                   \
        general_scroll_text_free(lines, (uint16_t) count);            \
        return NULL;                                                  \
      }                                                               \
      memcpy(_line, &text[_b], _e - _b);                              \
      _line[_e - _b] = '\0';                                          \
      lines[count++] = _line;                                         \
    }                                                                 \
  } while (0)

  size_t start = 0;
  size_t line_len = 0;

  /* Iterate to len + 1 so the last row is flushed at the terminator. */
  for (size_t i = 0; i <= len; i++) {
    const char c = text[i];
    if (c == '\0' || c == '\n') {
      EMIT_ROW(start, i);
      start = i + 1;
      line_len = 0;
      continue;
    }
    if (line_len >= GENERAL_SCROLL_MAX_LINE_LEN) {
      /* Emit [start, i) and let the *current* character open the next row.
         Starting the next row at i + 1 silently dropped one character per wrap.
       */
      EMIT_ROW(start, i);
      start = i;
      line_len = 0;
    }
    line_len++;
  }

#undef EMIT_ROW

  *num_lines = (uint16_t) count;
  return lines;
}
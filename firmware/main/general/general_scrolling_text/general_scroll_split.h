/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Line splitting for the scrolling-text screens.
 *
 * Kept free of ESP-IDF dependencies so the host test suite can exercise it: this
 * is the function that used to overflow a heap array on every Help / Credits /
 * Legal screen.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum characters per rendered row (the display is 128 px / 8 px per char). */
#define GENERAL_SCROLL_MAX_LINE_LEN 14

/**
 * @brief Split `text` into NUL-terminated rows of at most
 *        GENERAL_SCROLL_MAX_LINE_LEN characters.
 *
 * Splits on '\n' and wraps on the width limit. Lines that contain nothing but
 * whitespace are dropped.
 *
 * @param text      NUL-terminated input, may be NULL
 * @param num_lines receives the number of rows; never NULL
 * @return malloc'd array of `*num_lines` rows, which the caller frees with
 *         general_scroll_text_free(); NULL on allocation failure
 */
char** general_scroll_text_split(const char* text, uint16_t* num_lines);

/**
 * @brief Release an array returned by general_scroll_text_split()
 */
void general_scroll_text_free(char** lines, uint16_t num_lines);

#ifdef __cplusplus
}
#endif
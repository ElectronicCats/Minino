#pragma once

#include "sh1106.h"

#define INVERT    1
#define NO_INVERT 0

/**
 * @brief Initialize the OLED driver display
 *
 * @return void
 */
void oled_driver_init();

/**
 * @brief Clear the OLED display
 *
 * @return void
 */
void oled_driver_clear();
void oled_driver_display_text(const char* text, int x, int page, int invert);
void oled_driver_clear_line(int x, int page, int invert);

/**
 * @brief Display a bitmap on the OLED display
 *
 * @param bitmap
 * @param x
 * @param y
 * @param width
 * @param height
 * @param invert
 *
 * @return void
 */
void oled_driver_display_bitmap(const uint8_t* bitmap,
                                int x,
                                int y,
                                int width,
                                int height,
                                int invert);
void oled_driver_display_selected_item_box();

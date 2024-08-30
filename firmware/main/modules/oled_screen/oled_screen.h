#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "oled_driver.h"

#define OLED_DISPLAY_INVERT true
#define OLED_DISPLAY_NORMAL false
#define MAX_LINE_CHAR       16  // 16 * 8 = 128 -> width of the screen

#ifdef CONFIG_RESOLUTION_128X64
  #define MAX_PAGE 7  // 0 - 7
#else                 // CONFIG_RESOLUTION_128X32
  #define MAX_PAGE 3  // 0 - 3
#endif

/**
 * @brief Initialize the OLED driver display
 *
 * @return void
 */
void oled_screen_begin();

/**
 * @brief Clear the OLED display
 *
 * @return void
 */
void oled_screen_clear();

/**
 * @brief Show the content of the buffer on the OLED display
 *
 * @return void
 */
void oled_screen_display_show();

/**
 * @brief Display text on the OLED display
 *
 * @param text
 * @param x
 * @param page
 * @param invert
 *
 * @return void
 */
void oled_screen_display_text(char* text, int x, int page, bool invert);

/**
 * @brief Display the text on the center of the OLED display
 *
 * @param text Text to display on the OLED display
 * @param page Page to display the text on the OLED display
 * @param invert Invert the background and foreground color of the OLED display
 */
void oled_screen_display_text_center(char* text, int page, bool invert);

/**
 * @brief Clear a line on the OLED display
 *
 * @param x Offset from the left
 * @param page Page number, 0-7
 * @param invert
 *
 * @return void
 */
void oled_screen_clear_line(int x, int page, bool invert);

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
void oled_screen_display_bitmap(const uint8_t* bitmap,
                                int x,
                                int y,
                                int width,
                                int height,
                                bool invert);

/**
 * @brief Draw a pixel on the OLED display
 *
 * @param x
 * @param y
 * @param invert
 *
 * @return void
 */
void oled_screen_draw_pixel(int x, int y, bool invert);

/**
 * @brief Draw a rectangle on the OLED display
 *
 * @param x
 * @param y
 * @param width
 * @param height
 * @param invert
 *
 * @return void
 */
void oled_screen_draw_rect(int x, int y, int width, int height, bool invert);

/**
 * @brief Display the selected item box on the OLED display
 *
 * @return void
 */
void oled_screen_display_selected_item_box();

/**
 * @brief Display and split the text on the OLED display
 *
 * @param p_text Pointer of the text to display on the OLED display limited to
 * 50 char
 * @param p_started_page Pointer to the index of the page to display the text on
 * the OLED display
 * @param invert Invert the background and foreground color of the OLED display
 */
void oled_screen_display_text_splited(char* p_text,
                                      int* p_started_page,
                                      int invert);

void oled_screen_display_loading_bar(uint8_t value, uint8_t page);
void oled_screen_display_card_border();
void oled_screen_clear_buffer();
void oled_screen_display_show();
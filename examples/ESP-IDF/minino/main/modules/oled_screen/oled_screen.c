#include <string.h>

#include "bitmaps.h"
#include "esp_log.h"
#include "oled_screen.h"

static const char* TAG = "OLED_DRIVER";
oled_driver_t dev;

void oled_screen_begin() {
#if CONFIG_I2C_INTERFACE
  ESP_LOGI(TAG, "INTERFACE is i2c");
  ESP_LOGI(TAG, "CONFIG_SDA_GPIO=%d", CONFIG_SDA_GPIO);
  ESP_LOGI(TAG, "CONFIG_SCL_GPIO=%d", CONFIG_SCL_GPIO);
  ESP_LOGI(TAG, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);
  i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif  // CONFIG_I2C_INTERFACE

#if CONFIG_SPI_INTERFACE
  ESP_LOGI(TAG, "INTERFACE is SPI");
  ESP_LOGI(TAG, "CONFIG_MOSI_GPIO=%d", CONFIG_MOSI_GPIO);
  ESP_LOGI(TAG, "CONFIG_SCLK_GPIO=%d", CONFIG_SCLK_GPIO);
  ESP_LOGI(TAG, "CONFIG_CS_GPIO=%d", CONFIG_CS_GPIO);
  ESP_LOGI(TAG, "CONFIG_DC_GPIO=%d", CONFIG_DC_GPIO);
  ESP_LOGI(TAG, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);
  spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO,
                  CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
#endif  // CONFIG_SPI_INTERFACE

#if CONFIG_FLIP
  dev._flip = true;
  ESP_LOGW(TAG, "Flip upside down");
#endif

#if CONFIG_RESOLUTION_128X64
  ESP_LOGI(TAG, "PANEL: 128x64");
  oled_driver_init(&dev, 128, 64);
#elif CONFIG_RESOLUTION_128X32
  ESP_LOGI(TAG, "PANEL: 128x32");
  oled_driver_init(&dev, 128, 32);
#endif
}

void oled_screen_clear() {
  oled_driver_clear_screen(&dev, OLED_DISPLAY_NORMAL);
}

void oled_screen_display_show() {
  oled_driver_show_buffer(&dev);
}

void oled_screen_display_text(const char* text, int x, int page, bool invert) {
  oled_driver_display_text(&dev, page, text, x, invert);
}

void oled_screen_display_text_center(char* text, int page, bool invert) {
  int text_length = strlen(text);
  if (text_length > MAX_LINE_CHAR || true) {
    ESP_LOGE(TAG, "Text too long to center");
    oled_screen_display_text(text, 0, page, invert);
    return;
  }

  // We need to know if the text is odd or even
  int text_center = (MAX_LINE_CHAR - text_length) / 2;
  char text_centered[MAX_LINE_CHAR] = "";
  for (int i = 0; i < text_center; i++) {
    strcat(text_centered, " ");
  }
  strcat(text_centered, text);
  int text_centered_len = strlen(text_centered);
  for (int i = text_centered_len; i < MAX_LINE_CHAR; i++) {
    strcat(text_centered, " ");
  }
  oled_screen_display_text(text_centered, 0, page, invert);
}

void oled_screen_clear_line(int x, int page, bool invert) {
  // oled_driver_clear_line(&dev, x, page, invert);
  oled_driver_bitmaps(&dev, x, page * 8, epd_bitmap_clear_line, 128 - x, 8,
                      invert);
}

void oled_screen_display_bitmap(const uint8_t* bitmap,
                                int x,
                                int y,
                                int width,
                                int height,
                                bool invert) {
  oled_driver_bitmaps(&dev, x, y, bitmap, width, height, invert);
}

void oled_screen_draw_pixel(int x, int y, bool invert) {
  oled_driver_draw_pixel(&dev, x, y, invert);
}

void oled_screen_draw_rect(int x, int y, int width, int height, bool invert) {
  oled_driver_draw_rect(&dev, x, y, width, height, invert);
}

/// @brief Display a box around the selected item
void oled_screen_display_selected_item_box() {
  oled_driver_draw_custom_box(&dev);
}

void oled_screen_display_text_splited(char* p_text,
                                      int* p_started_page,
                                      int invert) {
  if (strlen(p_text) > MAX_LINE_CHAR) {
    char temp[50];
    strncpy(temp, p_text, 50);

    char* token = strtok(temp, " ");
    char current_line[MAX_LINE_CHAR] = "";
    while (token != NULL) {
      if (strlen(current_line) + strlen(token) + 1 <= MAX_LINE_CHAR) {
        if (strlen(current_line) > 0) {
          strcat(current_line, " ");
        }
        strcat(current_line, token);
      } else {
        oled_screen_display_text(current_line, 0, *p_started_page, invert);
        (*p_started_page)++;
        strcpy(current_line, token);
      }
      token = strtok(NULL, " ");
    }
    if (strlen(current_line) > 0) {
      oled_screen_display_text(current_line, 0, *p_started_page, invert);
      (*p_started_page)++;
    }
  } else {
    oled_screen_display_text(p_text, 0, *p_started_page, invert);
    (*p_started_page)++;
  }
}

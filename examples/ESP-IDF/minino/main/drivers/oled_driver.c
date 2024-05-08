#include "oled_driver.h"
#include "bitmaps.h"
#include "esp_log.h"

static const char* TAG = "OLED_DRIVER";
SH1106_t dev;

void oled_driver_init() {
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

#if CONFIG_SH1106_128x64
  ESP_LOGI(TAG, "Panel is 128x64");
  sh1106_init(&dev, 128, 64);
#endif  // CONFIG_SH1106_128x64
#if CONFIG_SH1106_128x32
  ESP_LOGI(TAG, "Panel is 128x32");
  sh1106_init(&dev, 128, 32);
#endif  // CONFIG_SH1106_128x32
}

void oled_driver_clear() {
  sh1106_clear_screen(&dev, false);
}

/// @brief Display text on the screen
/// @param text
/// @param text_size
/// @param page
/// @param invert
void oled_driver_display_text(const char* text, int x, int page, int invert) {
  sh1106_display_text(&dev, page, text, x, invert);
}

/// @brief Clear a line on the screen
/// @param page
/// @param invert
void oled_driver_clear_line(int x, int page, int invert) {
  // sh1106_clear_line(&dev, x, page, invert);
  sh1106_bitmaps(&dev, x, page * 8, epd_bitmap_clear_line, 128 - x, 8, invert);
}

/// @brief Display a bitmap on the screen
/// @param bitmap
/// @param x
/// @param y
/// @param width
/// @param height
/// @param invert
void oled_driver_display_bitmap(const uint8_t* bitmap,
                                int x,
                                int y,
                                int width,
                                int height,
                                int invert) {
  sh1106_bitmaps(&dev, x, y, bitmap, width, height, invert);
}

/// @brief Display a box around the selected item
void oled_driver_display_selected_item_box() {
  sh1106_draw_custom_box(&dev);
}

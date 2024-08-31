#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "oled_screen.h"

static const char* TAG = "OLED_DRIVER";
oled_driver_t dev;
SemaphoreHandle_t oled_mutex;

void oled_screen_begin() {
#if !defined(CONFIG_OLED_MODULE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  oled_mutex = xSemaphoreCreateMutex();
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
  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_clear_screen(&dev, OLED_DISPLAY_NORMAL);
  xSemaphoreGive(oled_mutex);
}

void oled_screen_display_show() {
  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_show_buffer(&dev);
  xSemaphoreGive(oled_mutex);
}
void oled_screen_clear_buffer() {
  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_clear_buffer(&dev);
  xSemaphoreGive(oled_mutex);
}

void oled_screen_display_text(char* text, int x, int page, bool invert) {
  if (text == NULL) {
    ESP_LOGE(TAG, "Text is NULL");
    return;
  }

  uint8_t _x = x + (strlen(text) * 8) > 128 ? 0 : x;
  if (_x != x) {
    ESP_LOGW(TAG, "Text %s is too long for the screen, x offset: %d", text, _x);
  }

  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_display_text(&dev, page, text, _x, invert);
  xSemaphoreGive(oled_mutex);
}

void oled_screen_display_text_center(char* text, int page, bool invert) {
  if (text == NULL) {
    ESP_LOGE(TAG, "Text is NULL");
    return;
  }

  int text_length = strlen(text);
  if (text_length > MAX_LINE_CHAR) {
    ESP_LOGE(TAG, "Text too long to center");
    oled_screen_display_text(text, 0, page, invert);
    return;
  }

  uint8_t middle_x_coordinate = 128 / 2;
  uint8_t half_text_length_px = (text_length * 8 / 2);
  uint8_t x_offset = middle_x_coordinate - half_text_length_px;
  oled_screen_display_text(text, x_offset, page, invert);
}

void oled_screen_clear_line(int x, int page, bool invert) {
  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_clear_line(&dev, x, page, invert);
  xSemaphoreGive(oled_mutex);
}

void oled_screen_display_bitmap(const uint8_t* bitmap,
                                int x,
                                int y,
                                int width,
                                int height,
                                bool invert) {
  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_bitmaps(&dev, x, y, bitmap, width, height, invert);
  xSemaphoreGive(oled_mutex);
}

void oled_screen_draw_pixel(int x, int y, bool invert) {
  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_draw_pixel(&dev, x, y, invert);
  xSemaphoreGive(oled_mutex);
}

void oled_screen_draw_rect(int x, int y, int width, int height, bool invert) {
  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_draw_rect(&dev, x, y, width, height, invert);
  xSemaphoreGive(oled_mutex);
}

/// @brief Display a box around the selected item
void oled_screen_display_selected_item_box() {
  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_draw_custom_box(&dev);
  xSemaphoreGive(oled_mutex);
}

void oled_screen_display_card_border() {
  xSemaphoreTake(oled_mutex, portMAX_DELAY);
  oled_driver_draw_modal_box(&dev, 0, 3);
  oled_driver_show_buffer(&dev);
  xSemaphoreGive(oled_mutex);
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
        oled_screen_display_text(current_line, 3, *p_started_page, invert);
        (*p_started_page)++;
        strcpy(current_line, token);
      }
      token = strtok(NULL, " ");
    }
    if (strlen(current_line) > 0) {
      oled_screen_display_text(current_line, 3, *p_started_page, invert);
      (*p_started_page)++;
    }
  } else {
    oled_screen_display_text(p_text, 3, *p_started_page, invert);
    (*p_started_page)++;
  }
}

void oled_screen_display_loading_bar(uint8_t value, uint8_t page) {
  uint8_t bar_bitmap[8][16];
  uint8_t active_cols = (uint32_t) value * 128 / 100;
  memset(bar_bitmap, 0, sizeof(bar_bitmap));
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < active_cols; x++) {
      bar_bitmap[y][x / 8] |= (1 << (7 - (x % 8)));
    }
  }
  oled_screen_display_bitmap(bar_bitmap, 0, page * 8, 128, 8,
                             OLED_DISPLAY_NORMAL);
}

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  OLED_DISPLAY_NORMAL = 0,
  OLED_DISPLAY_INVERT = 1,
} oled_display_mode_t;

typedef struct oled_driver oled_driver_t;

esp_err_t i2c_master_init(oled_driver_t* dev, int sda, int scl, int rst);
esp_err_t spi_master_init(oled_driver_t* dev,
                          int mosi,
                          int sclk,
                          int cs,
                          int dc,
                          int rst);
esp_err_t oled_driver_init(oled_driver_t* dev, int width, int height);
void oled_driver_clear_screen(oled_driver_t* dev, oled_display_mode_t mode);
void oled_driver_clear_buffer(oled_driver_t* dev);
void oled_driver_show_buffer(oled_driver_t* dev);
void oled_driver_set_buffer(oled_driver_t* dev, uint8_t* buffer);
void oled_driver_get_buffer(oled_driver_t* dev, uint8_t* buffer);
void oled_driver_display_text(oled_driver_t* dev,
                              int page,
                              const char* text,
                              int x,
                              bool invert);
void oled_driver_display_bitmap(oled_driver_t* dev,
                                int x,
                                int y,
                                const uint8_t* bitmap,
                                int w,
                                int h,
                                oled_display_mode_t mode);
void oled_driver_display_image(oled_driver_t* dev,
                               int page,
                               int seg,
                               const uint8_t* images,
                               int width);
int oled_driver_get_width(oled_driver_t* dev);
int oled_driver_get_height(oled_driver_t* dev);
int oled_driver_get_pages(oled_driver_t* dev);

#ifdef __cplusplus
}
#endif
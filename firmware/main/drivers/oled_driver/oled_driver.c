#include <string.h>

#include "esp_log.h"
#include "font8x8_basic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oled_driver.h"

#define PACK8 __attribute__((aligned(__alignof__(uint8_t)), packed))

static const char* TAG = "oled_driver";

typedef union out_column_t {
  uint32_t u32;
  uint8_t u8[4];
} PACK8 out_column_t;

void oled_driver_init(oled_driver_t* dev, int width, int height) {
#if !defined(CONFIG_OLED_DRIVER_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  if (dev->_address == SPIAddress) {
    spi_init(dev, width, height);
  } else {
    i2c_init(dev, width, height);
  }
  // Initialize internal buffer
  for (int i = 0; i < dev->_pages; i++) {
    memset(dev->_page[i]._segs, 0, width);
  }
}

int oled_driver_get_width(oled_driver_t* dev) {
  return dev->_width;
}

int oled_driver_get_height(oled_driver_t* dev) {
  return dev->_height;
}

int oled_driver_get_pages(oled_driver_t* dev) {
  return dev->_pages;
}

void oled_driver_show_buffer(oled_driver_t* dev) {
  if (dev->_address == SPIAddress) {
    for (int page = 0; page < dev->_pages; page++) {
      spi_display_image(dev, page, 0, dev->_page[page]._segs, dev->_width);
    }
  } else {
    for (int page = 0; page < dev->_pages; page++) {
      i2c_display_image(dev, page, 0, dev->_page[page]._segs, dev->_width);
    }
  }
}

void oled_driver_set_buffer(oled_driver_t* dev, uint8_t* buffer) {
  int index = 0;
  for (int page = 0; page < dev->_pages; page++) {
    memcpy(&dev->_page[page]._segs, &buffer[index], dev->_width);
    index = index + dev->_width;
  }
}

void oled_driver_get_buffer(oled_driver_t* dev, uint8_t* buffer) {
  int index = 0;
  for (int page = 0; page < dev->_pages; page++) {
    memcpy(&buffer[index], &dev->_page[page]._segs, dev->_width);
    index = index + dev->_width;
  }
}

void oled_driver_display_image(oled_driver_t* dev,
                               int page,
                               int seg,
                               uint8_t* images,
                               int width) {
  if (dev->_address == SPIAddress) {
    spi_display_image(dev, page, seg, images, width);
  } else {
    i2c_display_image(dev, page, seg, images, width);
  }
  // Set to internal buffer
  memcpy(&dev->_page[page]._segs[seg], images, width);
}

void oled_driver_display_text(oled_driver_t* dev,
                              int page,
                              char* text,
                              int x,
                              bool invert) {
  if (page >= dev->_pages)
    return;
  int _text_len = strlen(text);
  if (_text_len > 16)
    _text_len = 16;

  uint8_t seg = x;
  uint8_t image[8];
  for (uint8_t i = 0; i < _text_len; i++) {
    memcpy(image, font8x8_basic_tr[(uint8_t) text[i]], 8);
    if (invert)
      oled_driver_invert(image, 8);
    if (dev->_flip)
      oled_driver_flip(image, 8);
    oled_driver_display_image(dev, page, seg, image, 8);
#if 0
		if (dev->_address == SPIAddress) {
			spi_display_image(dev, page, seg, image, 8);
		} else {
			i2c_display_image(dev, page, seg, image, 8);
		}
#endif
    seg = seg + 8;
  }
}

// by Coert Vonk
void oled_driver_display_text_x3(oled_driver_t* dev,
                                 int page,
                                 char* text,
                                 int text_len,
                                 bool invert) {
  if (page >= dev->_pages)
    return;
  int _text_len = text_len;
  if (_text_len > 5)
    _text_len = 5;

  uint8_t seg = 0;

  for (uint8_t nn = 0; nn < _text_len; nn++) {
    uint8_t const* const in_columns = font8x8_basic_tr[(uint8_t) text[nn]];

    // make the character 3x as high
    out_column_t out_columns[8];
    memset(out_columns, 0, sizeof(out_columns));

    for (uint8_t xx = 0; xx < 8; xx++) {  // for each column (x-direction)

      uint32_t in_bitmask = 0b1;
      uint32_t out_bitmask = 0b111;

      for (uint8_t yy = 0; yy < 8; yy++) {  // for pixel (y-direction)
        if (in_columns[xx] & in_bitmask) {
          out_columns[xx].u32 |= out_bitmask;
        }
        in_bitmask <<= 1;
        out_bitmask <<= 3;
      }
    }

    // render character in 8 column high pieces, making them 3x as wide
    for (uint8_t yy = 0; yy < 3;
         yy++) {  // for each group of 8 pixels high (y-direction)

      uint8_t image[24];
      for (uint8_t xx = 0; xx < 8; xx++) {  // for each column (x-direction)
        image[xx * 3 + 0] = image[xx * 3 + 1] = image[xx * 3 + 2] =
            out_columns[xx].u8[yy];
      }
      if (invert)
        oled_driver_invert(image, 24);
      if (dev->_flip)
        oled_driver_flip(image, 24);
      if (dev->_address == SPIAddress) {
        spi_display_image(dev, page + yy, seg, image, 24);
      } else {
        i2c_display_image(dev, page + yy, seg, image, 24);
      }
      memcpy(&dev->_page[page + yy]._segs[seg], image, 24);
    }
    seg = seg + 24;
  }
}

void oled_driver_clear_buffer(oled_driver_t* dev) {
  for (int i = 0; i < dev->_pages; i++) {
    memset(dev->_page[i]._segs, 0, dev->_width);
  }
}

void oled_driver_clear_screen(oled_driver_t* dev, bool invert) {
  oled_driver_clear_buffer(dev);
  oled_driver_show_buffer(dev);
}

void oled_driver_clear_line(oled_driver_t* dev, int x, int page, bool invert) {
  // char space[16];
  // memset(space, 0x00, sizeof(space));
  // oled_driver_display_text(dev, page, space, sizeof(space), invert);
  char* space = "                ";
  oled_driver_display_text(dev, page, space, x, invert);
}

void oled_driver_contrast(oled_driver_t* dev, int contrast) {
  if (dev->_address == SPIAddress) {
    spi_contrast(dev, contrast);
  } else {
    i2c_contrast(dev, contrast);
  }
}

void oled_driver_software_scroll(oled_driver_t* dev, int start, int end) {
  ESP_LOGD(TAG, "software_scroll start=%d end=%d _pages=%d", start, end,
           dev->_pages);
  if (start < 0 || end < 0) {
    dev->_scEnable = false;
  } else if (start >= dev->_pages || end >= dev->_pages) {
    dev->_scEnable = false;
  } else {
    dev->_scEnable = true;
    dev->_scStart = start;
    dev->_scEnd = end;
    dev->_scDirection = 1;
    if (start > end)
      dev->_scDirection = -1;
  }
}

void oled_driver_scroll_text(oled_driver_t* dev,
                             char* text,
                             int text_len,
                             bool invert) {
  ESP_LOGD(TAG, "dev->_scEnable=%d", dev->_scEnable);
  if (dev->_scEnable == false)
    return;

  void (*func)(oled_driver_t* dev, int page, int seg, uint8_t* images,
               int width);
  if (dev->_address == SPIAddress) {
    func = spi_display_image;
  } else {
    func = i2c_display_image;
  }

  int srcIndex = dev->_scEnd - dev->_scDirection;
  while (1) {
    int dstIndex = srcIndex + dev->_scDirection;
    ESP_LOGD(TAG, "srcIndex=%d dstIndex=%d", srcIndex, dstIndex);
    for (int seg = 0; seg < dev->_width; seg++) {
      dev->_page[dstIndex]._segs[seg] = dev->_page[srcIndex]._segs[seg];
    }
    (*func)(dev, dstIndex, 0, dev->_page[dstIndex]._segs,
            sizeof(dev->_page[dstIndex]._segs));
    if (srcIndex == dev->_scStart)
      break;
    srcIndex = srcIndex - dev->_scDirection;
  }

  int _text_len = text_len;
  if (_text_len > 16)
    _text_len = 16;

  oled_driver_display_text(dev, srcIndex, text, text_len, invert);
}

void oled_driver_scroll_clear(oled_driver_t* dev) {
  ESP_LOGD(TAG, "dev->_scEnable=%d", dev->_scEnable);
  if (dev->_scEnable == false)
    return;

  int srcIndex = dev->_scEnd - dev->_scDirection;
  while (1) {
    int dstIndex = srcIndex + dev->_scDirection;
    ESP_LOGD(TAG, "srcIndex=%d dstIndex=%d", srcIndex, dstIndex);
    oled_driver_clear_line(dev, 0, dstIndex, false);
    if (dstIndex == dev->_scStart)
      break;
    srcIndex = srcIndex - dev->_scDirection;
  }
}

void oled_driver_hardware_scroll(oled_driver_t* dev,
                                 oled_driver_scroll_type_t scroll) {
  if (dev->_address == SPIAddress) {
    spi_hardware_scroll(dev, scroll);
  } else {
    i2c_hardware_scroll(dev, scroll);
  }
}

// delay = 0 : display with no wait
// delay > 0 : display with wait
// delay < 0 : no display
void oled_driver_wrap_arround(oled_driver_t* dev,
                              oled_driver_scroll_type_t scroll,
                              int start,
                              int end,
                              int8_t delay) {
  if (scroll == SCROLL_RIGHT) {
    int _start = start;  // 0 to 7
    int _end = end;      // 0 to 7
    if (_end >= dev->_pages)
      _end = dev->_pages - 1;
    uint8_t wk;
    // for (int page=0;page<dev->_pages;page++) {
    for (int page = _start; page <= _end; page++) {
      wk = dev->_page[page]._segs[127];
      for (int seg = 127; seg > 0; seg--) {
        dev->_page[page]._segs[seg] = dev->_page[page]._segs[seg - 1];
      }
      dev->_page[page]._segs[0] = wk;
    }

  } else if (scroll == SCROLL_LEFT) {
    int _start = start;  // 0 to 7
    int _end = end;      // 0 to 7
    if (_end >= dev->_pages)
      _end = dev->_pages - 1;
    uint8_t wk;
    // for (int page=0;page<dev->_pages;page++) {
    for (int page = _start; page <= _end; page++) {
      wk = dev->_page[page]._segs[0];
      for (int seg = 0; seg < 127; seg++) {
        dev->_page[page]._segs[seg] = dev->_page[page]._segs[seg + 1];
      }
      dev->_page[page]._segs[127] = wk;
    }

  } else if (scroll == SCROLL_UP) {
    int _start = start;  // 0 to {width-1}
    int _end = end;      // 0 to {width-1}
    if (_end >= dev->_width)
      _end = dev->_width - 1;
    uint8_t wk0;
    uint8_t wk1;
    uint8_t wk2;
    uint8_t save[dev->_width];
    // Save pages 0
    for (int seg = 0; seg < dev->_width; seg++) {
      save[seg] = dev->_page[0]._segs[seg];
    }
    // Page0 to Page6
    for (int page = 0; page < dev->_pages - 1; page++) {
      // for (int seg=0;seg<dev->_width;seg++) {
      for (int seg = _start; seg <= _end; seg++) {
        wk0 = dev->_page[page]._segs[seg];
        wk1 = dev->_page[page + 1]._segs[seg];
        if (dev->_flip)
          wk0 = oled_driver_rotate_byte(wk0);
        if (dev->_flip)
          wk1 = oled_driver_rotate_byte(wk1);
        if (seg == 0) {
          ESP_LOGD(TAG, "b page=%d wk0=%02x wk1=%02x", page, wk0, wk1);
        }
        wk0 = wk0 >> 1;
        wk1 = wk1 & 0x01;
        wk1 = wk1 << 7;
        wk2 = wk0 | wk1;
        if (seg == 0) {
          ESP_LOGD(TAG, "a page=%d wk0=%02x wk1=%02x wk2=%02x", page, wk0, wk1,
                   wk2);
        }
        if (dev->_flip)
          wk2 = oled_driver_rotate_byte(wk2);
        dev->_page[page]._segs[seg] = wk2;
      }
    }
    // Page7
    int pages = dev->_pages - 1;
    // for (int seg=0;seg<dev->_width;seg++) {
    for (int seg = _start; seg <= _end; seg++) {
      wk0 = dev->_page[pages]._segs[seg];
      wk1 = save[seg];
      if (dev->_flip)
        wk0 = oled_driver_rotate_byte(wk0);
      if (dev->_flip)
        wk1 = oled_driver_rotate_byte(wk1);
      wk0 = wk0 >> 1;
      wk1 = wk1 & 0x01;
      wk1 = wk1 << 7;
      wk2 = wk0 | wk1;
      if (dev->_flip)
        wk2 = oled_driver_rotate_byte(wk2);
      dev->_page[pages]._segs[seg] = wk2;
    }

  } else if (scroll == SCROLL_DOWN) {
    int _start = start;  // 0 to {width-1}
    int _end = end;      // 0 to {width-1}
    if (_end >= dev->_width)
      _end = dev->_width - 1;
    uint8_t wk0;
    uint8_t wk1;
    uint8_t wk2;
    uint8_t save[dev->_width];
    // Save pages 7
    int pages = dev->_pages - 1;
    for (int seg = 0; seg < dev->_width; seg++) {
      save[seg] = dev->_page[pages]._segs[seg];
    }
    // Page7 to Page1
    for (int page = pages; page > 0; page--) {
      // for (int seg=0;seg<dev->_width;seg++) {
      for (int seg = _start; seg <= _end; seg++) {
        wk0 = dev->_page[page]._segs[seg];
        wk1 = dev->_page[page - 1]._segs[seg];
        if (dev->_flip)
          wk0 = oled_driver_rotate_byte(wk0);
        if (dev->_flip)
          wk1 = oled_driver_rotate_byte(wk1);
        if (seg == 0) {
          ESP_LOGD(TAG, "b page=%d wk0=%02x wk1=%02x", page, wk0, wk1);
        }
        wk0 = wk0 << 1;
        wk1 = wk1 & 0x80;
        wk1 = wk1 >> 7;
        wk2 = wk0 | wk1;
        if (seg == 0) {
          ESP_LOGD(TAG, "a page=%d wk0=%02x wk1=%02x wk2=%02x", page, wk0, wk1,
                   wk2);
        }
        if (dev->_flip)
          wk2 = oled_driver_rotate_byte(wk2);
        dev->_page[page]._segs[seg] = wk2;
      }
    }
    // Page0
    // for (int seg=0;seg<dev->_width;seg++) {
    for (int seg = _start; seg <= _end; seg++) {
      wk0 = dev->_page[0]._segs[seg];
      wk1 = save[seg];
      if (dev->_flip)
        wk0 = oled_driver_rotate_byte(wk0);
      if (dev->_flip)
        wk1 = oled_driver_rotate_byte(wk1);
      wk0 = wk0 << 1;
      wk1 = wk1 & 0x80;
      wk1 = wk1 >> 7;
      wk2 = wk0 | wk1;
      if (dev->_flip)
        wk2 = oled_driver_rotate_byte(wk2);
      dev->_page[0]._segs[seg] = wk2;
    }
  }

  if (delay >= 0) {
    for (int page = 0; page < dev->_pages; page++) {
      if (dev->_address == SPIAddress) {
        spi_display_image(dev, page, 0, dev->_page[page]._segs, dev->_width);
      } else {
        i2c_display_image(dev, page, 0, dev->_page[page]._segs, dev->_width);
      }
      if (delay)
        vTaskDelay(delay);
    }
  }
}

void oled_driver_bitmaps(oled_driver_t* dev,
                         int xpos,
                         int ypos,
                         uint8_t* bitmap,
                         int width,
                         int height,
                         bool invert) {
  if ((width % 8) != 0) {
    ESP_LOGE(TAG, "width must be a multiple of 8");
    return;
  }
  xpos += 2;
  int _width = width / 8;
  uint8_t wk0;
  uint8_t wk1;
  uint8_t wk2;
  uint8_t page = (ypos / 8);
  uint8_t _seg = xpos;
  uint8_t dstBits = (ypos % 8);
  ESP_LOGD(TAG, "ypos=%d page=%d dstBits=%d", ypos, page, dstBits);
  int offset = 0;
  for (int _height = 0; _height < height; _height++) {
    for (int index = 0; index < _width; index++) {
      for (int srcBits = 7; srcBits >= 0; srcBits--) {
        wk0 = dev->_page[page]._segs[_seg];
        if (dev->_flip)
          wk0 = oled_driver_rotate_byte(wk0);

        wk1 = bitmap[index + offset];
        if (invert)
          wk1 = ~wk1;

        // wk2 = oled_driver_copy_bit(bitmap[index+offset], srcBits, wk0,
        // dstBits);
        wk2 = oled_driver_copy_bit(wk1, srcBits, wk0, dstBits);
        if (dev->_flip)
          wk2 = oled_driver_rotate_byte(wk2);

        ESP_LOGD(TAG, "index=%d offset=%d page=%d _seg=%d, wk2=%02x", index,
                 offset, page, _seg, wk2);
        dev->_page[page]._segs[_seg] = wk2;
        _seg++;
      }
    }
    offset = offset + _width;
    dstBits++;
    _seg = xpos;
    if (dstBits == 8) {
      page++;
      dstBits = 0;
    }
  }

#if 0
	for (int _seg=ypos;_seg<ypos+width;_seg++) {
		oled_driver_dump_page(dev, page-1, _seg);
	}
	for (int _seg=ypos;_seg<ypos+width;_seg++) {
		oled_driver_dump_page(dev, page, _seg);
	}
#endif
  oled_driver_show_buffer(dev);
}

// Set pixel to internal buffer. Not show it.
void oled_driver_draw_pixel(oled_driver_t* dev,
                            int xpos,
                            int ypos,
                            bool invert) {
  uint8_t _page = (ypos / 8);
  uint8_t _bits = (ypos % 8);
  uint8_t _seg = xpos;
  uint8_t wk0 = dev->_page[_page]._segs[_seg];
  uint8_t wk1 = 1 << _bits;
  ESP_LOGD(TAG, "ypos=%d _page=%d _bits=%d wk0=0x%02x wk1=0x%02x", ypos, _page,
           _bits, wk0, wk1);
  if (invert) {
    wk0 = wk0 & ~wk1;
  } else {
    wk0 = wk0 | wk1;
  }
  if (dev->_flip)
    wk0 = oled_driver_rotate_byte(wk0);
  ESP_LOGD(TAG, "wk0=0x%02x wk1=0x%02x", wk0, wk1);
  dev->_page[_page]._segs[_seg] = wk0;
}

// Set line to internal buffer. Not show it.
void _oled_driver_line(oled_driver_t* dev,
                       int x1,
                       int y1,
                       int x2,
                       int y2,
                       bool invert) {
  int i;
  int dx, dy;
  int sx, sy;
  int E;

  /* distance between two points */
  dx = (x2 > x1) ? x2 - x1 : x1 - x2;
  dy = (y2 > y1) ? y2 - y1 : y1 - y2;

  /* direction of two point */
  sx = (x2 > x1) ? 1 : -1;
  sy = (y2 > y1) ? 1 : -1;

  /* inclination < 1 */
  if (dx > dy) {
    E = -dx;
    for (i = 0; i <= dx; i++) {
      oled_driver_draw_pixel(dev, x1, y1, invert);
      x1 += sx;
      E += 2 * dy;
      if (E >= 0) {
        y1 += sy;
        E -= 2 * dx;
      }
    }

    /* inclination >= 1 */
  } else {
    E = -dy;
    for (i = 0; i <= dy; i++) {
      oled_driver_draw_pixel(dev, x1, y1, invert);
      y1 += sy;
      E += 2 * dx;
      if (E >= 0) {
        x1 += sx;
        E -= 2 * dy;
      }
    }
  }
}

void oled_driver_invert(uint8_t* buf, size_t blen) {
  uint8_t wk;
  for (int i = 0; i < blen; i++) {
    wk = buf[i];
    buf[i] = ~wk;
  }
}

// Flip upside down
void oled_driver_flip(uint8_t* buf, size_t blen) {
  for (int i = 0; i < blen; i++) {
    buf[i] = oled_driver_rotate_byte(buf[i]);
  }
}

uint8_t oled_driver_copy_bit(uint8_t src,
                             int srcBits,
                             uint8_t dst,
                             int dstBits) {
  ESP_LOGD(TAG, "src=%02x srcBits=%d dst=%02x dstBits=%d", src, srcBits, dst,
           dstBits);
  uint8_t smask = 0x01 << srcBits;
  uint8_t dmask = 0x01 << dstBits;
  uint8_t _src = src & smask;
#if 0
	if (_src != 0) _src = 1;
	uint8_t _wk = _src << dstBits;
	uint8_t _dst = dst | _wk;
#endif
  uint8_t _dst;
  if (_src != 0) {
    _dst = dst | dmask;  // set bit
  } else {
    _dst = dst & ~(dmask);  // clear bit
  }
  return _dst;
}

// Rotate 8-bit data
// 0x12-->0x48
uint8_t oled_driver_rotate_byte(uint8_t ch1) {
  uint8_t ch2 = 0;
  for (int j = 0; j < 8; j++) {
    ch2 = (ch2 << 1) + (ch1 & 0x01);
    ch1 = ch1 >> 1;
  }
  return ch2;
}

void oled_driver_fadeout(oled_driver_t* dev) {
  void (*func)(oled_driver_t* dev, int page, int seg, uint8_t* images,
               int width);
  if (dev->_address == SPIAddress) {
    func = spi_display_image;
  } else {
    func = i2c_display_image;
  }

  uint8_t image[1];
  for (int page = 0; page < dev->_pages; page++) {
    image[0] = 0xFF;
    for (int line = 0; line < 8; line++) {
      if (dev->_flip) {
        image[0] = image[0] >> 1;
      } else {
        image[0] = image[0] << 1;
      }
      for (int seg = 0; seg < dev->_width; seg++) {
        (*func)(dev, page, seg, image, 1);
        dev->_page[page]._segs[seg] = image[0];
      }
    }
  }
}

void oled_driver_dump(oled_driver_t dev) {
  printf("_address=%x\n", dev._address);
  printf("_width=%x\n", dev._width);
  printf("_height=%x\n", dev._height);
  printf("_pages=%x\n", dev._pages);
}

void oled_driver_dump_page(oled_driver_t* dev, int page, int seg) {
  ESP_LOGI(TAG, "dev->_page[%d]._segs[%d]=%02x", page, seg,
           dev->_page[page]._segs[seg]);
}

void oled_driver_draw_line(oled_driver_t* dev,
                           int x1,
                           int y1,
                           int x2,
                           int y2,
                           bool invert) {
  _oled_driver_line(dev, x1, y1, x2, y2, invert);
  // oled_driver_show_buffer(dev);
}

void oled_driver_draw_hline(oled_driver_t* dev,
                            int x,
                            int y,
                            int width,
                            bool invert) {
  for (int i = 0; i < width; i++) {
    oled_driver_draw_pixel(dev, x + i, y, invert);
  }
  // oled_driver_show_buffer(dev);
}

void oled_driver_draw_vline(oled_driver_t* dev,
                            int x,
                            int y,
                            int height,
                            bool invert) {
  for (int i = 0; i < height; i++) {
    oled_driver_draw_pixel(dev, x, y + i, invert);
  }
  // oled_driver_show_buffer(dev);
}

void oled_driver_draw_rect(oled_driver_t* dev,
                           int x,
                           int y,
                           int width,
                           int height,
                           bool invert) {
  oled_driver_draw_hline(dev, x, y, width, invert);               // Top
  oled_driver_draw_hline(dev, x, y + height - 1, width, invert);  // Bottom
  oled_driver_draw_vline(dev, x, y, height, invert);              // Left
  oled_driver_draw_vline(dev, x + width - 1, y, height, invert);  // Right
  // oled_driver_show_buffer(dev);
}

void oled_driver_draw_custom_box(oled_driver_t* dev) {
  int page = 3;
  int x = 0;
  int y = page * 8 - 3;
  int width = x + dev->_width - 4;
  int height = y - 6;

  oled_driver_draw_rect(dev, x, y, width, height, 0);
  oled_driver_draw_rect(dev, x, y, width - 1, height - 1, 0);

  // Top left border
  oled_driver_draw_pixel(dev, x, y, 1);
  oled_driver_draw_pixel(dev, x + 1, y, 1);
  oled_driver_draw_pixel(dev, x, y + 1, 1);
  oled_driver_draw_pixel(dev, x + 1, y + 1, 0);

  // Top right border
  oled_driver_draw_pixel(dev, width - 1, y, 1);
  oled_driver_draw_pixel(dev, width - 2, y, 1);
  oled_driver_draw_pixel(dev, width - 1, y + 1, 1);
  oled_driver_draw_pixel(dev, width - 2, y + 1, 0);

  // Bottom left border
  oled_driver_draw_pixel(dev, x, y + height - 1, 1);
  oled_driver_draw_pixel(dev, x + 1, y + height - 1, 1);
  oled_driver_draw_pixel(dev, x, y + height - 2, 1);
  oled_driver_draw_pixel(dev, x + 1, y + height - 2, 0);

  // Bottom right border
  oled_driver_draw_pixel(dev, width - 1, y + height - 1, 1);
  oled_driver_draw_pixel(dev, width - 2, y + height - 1, 1);
  oled_driver_draw_pixel(dev, width - 1, y + height - 2, 1);
  oled_driver_draw_pixel(dev, width - 2, y + height - 2, 0);

  // oled_driver_show_buffer(dev);
}

void oled_driver_draw_modal_box(oled_driver_t* dev,
                                int pos_x,
                                int modal_height) {
#ifdef CONFIG_RESOLUTION_128X64
  int initial_page = 2;
  int height_offset = 35;
  int y_offset = 3;
#else
  int initial_page = 1;
  int height_offset = 18;
  int y_offset = 1;
#endif
  int page = initial_page;
  int x = pos_x;
  int y = page * 8 - y_offset;  // 13
  int width = x + dev->_width - 4;
  int height = y + height_offset;  //- 6; // 15

  oled_driver_draw_rect(dev, x, y, width, height, 0);
  oled_driver_draw_rect(dev, x, y, width - 1, height - 1, 0);

  // Top left border
  oled_driver_draw_pixel(dev, x, y, 1);
  oled_driver_draw_pixel(dev, x + 1, y, 1);
  oled_driver_draw_pixel(dev, x, y + 1, 1);
  oled_driver_draw_pixel(dev, x + 1, y + 1, 0);

  // Top right border
  oled_driver_draw_pixel(dev, width - 1, y, 1);
  oled_driver_draw_pixel(dev, width - 2, y, 1);
  oled_driver_draw_pixel(dev, width - 1, y + 1, 1);
  oled_driver_draw_pixel(dev, width - 2, y + 1, 0);

  // Bottom left border
  oled_driver_draw_pixel(dev, x, y + height - 1, 1);
  oled_driver_draw_pixel(dev, x + 1, y + height - 1, 1);
  oled_driver_draw_pixel(dev, x, y + height - 2, 1);
  oled_driver_draw_pixel(dev, x + 1, y + height - 2, 0);

  // Bottom right border
  oled_driver_draw_pixel(dev, width - 1, y + height - 1, 1);
  oled_driver_draw_pixel(dev, width - 2, y + height - 1, 1);
  oled_driver_draw_pixel(dev, width - 1, y + height - 2, 1);
  oled_driver_draw_pixel(dev, width - 2, y + height - 2, 0);

  // oled_driver_show_buffer(dev);
}

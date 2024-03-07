#ifndef MAIN_SH1106_H_
#define MAIN_SH1106_H_

#include "driver/spi_master.h"

// Following definitions are bollowed from
// http://robotcantalk.blogspot.com/2015/03/interfacing-arduino-with-sh1106-driven.html

/* Control byte for i2c
Co : bit 8 : Continuation Bit
 * 1 = no-continuation (only one byte to follow)
 * 0 = the controller should expect a stream of bytes.
D/C# : bit 7 : Data/Command Select bit
 * 1 = the next byte or byte stream will be Data.
 * 0 = a Command byte or byte stream will be coming up next.
 Bits 6-0 will be all zeros.
Usage:
0x80 : Single Command byte
0x00 : Command Stream
0xC0 : Single Data byte
0x40 : Data Stream
*/
#define OLED_CONTROL_BYTE_CMD_SINGLE 0x80
#define OLED_CONTROL_BYTE_CMD_STREAM 0x00
#define OLED_CONTROL_BYTE_DATA_SINGLE 0xC0
#define OLED_CONTROL_BYTE_DATA_STREAM 0x40

// Fundamental commands (pg.28)
#define OLED_CMD_SET_CONTRAST 0x81  // follow with 0x7F
#define OLED_CMD_DISPLAY_RAM 0xA4
#define OLED_CMD_DISPLAY_ALLON 0xA5
#define OLED_CMD_DISPLAY_NORMAL 0xA6
#define OLED_CMD_DISPLAY_INVERTED 0xA7
#define OLED_CMD_DISPLAY_OFF 0xAE
#define OLED_CMD_DISPLAY_ON 0xAF

// Addressing Command Table (pg.30)
#define OLED_CMD_SET_MEMORY_ADDR_MODE 0x20
#define OLED_CMD_SET_HORI_ADDR_MODE 0x00  // Horizontal Addressing Mode
#define OLED_CMD_SET_VERT_ADDR_MODE 0x01  // Vertical Addressing Mode
#define OLED_CMD_SET_PAGE_ADDR_MODE 0x02  // Page Addressing Mode
#define OLED_CMD_SET_COLUMN_RANGE 0x21    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F = COL127
#define OLED_CMD_SET_PAGE_RANGE 0x22      // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7

// Hardware Config (pg.31)
#define OLED_CMD_SET_DISPLAY_START_LINE 0x40
#define OLED_CMD_SET_SEGMENT_REMAP_0 0xA0
#define OLED_CMD_SET_SEGMENT_REMAP_1 0xA1
#define OLED_CMD_SET_MUX_RATIO 0xA8  // follow with 0x3F = 64 MUX
#define OLED_CMD_SET_COM_SCAN_MODE 0xC8
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3  // follow with 0x00
#define OLED_CMD_SET_COM_PIN_MAP 0xDA     // follow with 0x12
#define OLED_CMD_NOP 0xE3                 // NOP

// Timing and Driving Scheme (pg.32)
#define OLED_CMD_SET_DISPLAY_CLK_DIV 0xD5  // follow with 0x80
#define OLED_CMD_SET_PRECHARGE 0xD9        // follow with 0xF1
#define OLED_CMD_SET_VCOMH_DESELCT 0xDB    // follow with 0x30

// Charge Pump (pg.62)
#define OLED_CMD_SET_CHARGE_PUMP 0x8D  // follow with 0x14

// Scrolling Command
#define OLED_CMD_HORIZONTAL_RIGHT 0x26
#define OLED_CMD_HORIZONTAL_LEFT 0x27
#define OLED_CMD_CONTINUOUS_SCROLL 0x29
#define OLED_CMD_DEACTIVE_SCROLL 0x2E
#define OLED_CMD_ACTIVE_SCROLL 0x2F
#define OLED_CMD_VERTICAL 0xA3

#define I2CAddress 0x3C
#define SPIAddress 0xFF

typedef enum {
    SCROLL_RIGHT = 1,
    SCROLL_LEFT = 2,
    SCROLL_DOWN = 3,
    SCROLL_UP = 4,
    SCROLL_STOP = 5
} sh1106_scroll_type_t;

typedef struct {
    bool _valid;  // Not using it anymore
    int _segLen;  // Not using it anymore
    uint8_t _segs[132];
} PAGE_t;

typedef struct {
    int _address;
    int _width;
    int _height;
    int _pages;
    int _dc;
    spi_device_handle_t _SPIHandle;
    bool _scEnable;
    int _scStart;
    int _scEnd;
    int _scDirection;
    PAGE_t _page[8];
    bool _flip;
} SH1106_t;

#ifdef __cplusplus
extern "C" {
#endif

void sh1106_init(SH1106_t* dev, int width, int height);
int sh1106_get_width(SH1106_t* dev);
int sh1106_get_height(SH1106_t* dev);
int sh1106_get_pages(SH1106_t* dev);
void sh1106_show_buffer(SH1106_t* dev);
void sh1106_set_buffer(SH1106_t* dev, uint8_t* buffer);
void sh1106_get_buffer(SH1106_t* dev, uint8_t* buffer);
void sh1106_display_image(SH1106_t* dev, int page, int seg, uint8_t* images, int width);
void sh1106_display_text(SH1106_t* dev, int page, char* text, int text_len, bool invert);
void sh1106_display_text_x3(SH1106_t* dev, int page, char* text, int text_len, bool invert);
void sh1106_clear_screen(SH1106_t* dev, bool invert);
void sh1106_clear_line(SH1106_t* dev, int page, bool invert);
void sh1106_contrast(SH1106_t* dev, int contrast);
void sh1106_software_scroll(SH1106_t* dev, int start, int end);
void sh1106_scroll_text(SH1106_t* dev, char* text, int text_len, bool invert);
void sh1106_scroll_clear(SH1106_t* dev);
void sh1106_hardware_scroll(SH1106_t* dev, sh1106_scroll_type_t scroll);
void sh1106_wrap_arround(SH1106_t* dev, sh1106_scroll_type_t scroll, int start, int end, int8_t delay);
void sh1106_bitmaps(SH1106_t* dev, int xpos, int ypos, uint8_t* bitmap, int width, int height, bool invert);
void _sh1106_pixel(SH1106_t* dev, int xpos, int ypos, bool invert);
void _sh1106_line(SH1106_t* dev, int x1, int y1, int x2, int y2, bool invert);
void sh1106_invert(uint8_t* buf, size_t blen);
void sh1106_flip(uint8_t* buf, size_t blen);
uint8_t sh1106_copy_bit(uint8_t src, int srcBits, uint8_t dst, int dstBits);
uint8_t sh1106_rotate_byte(uint8_t ch1);
void sh1106_fadeout(SH1106_t* dev);
void sh1106_dump(SH1106_t dev);
void sh1106_dump_page(SH1106_t* dev, int page, int seg);
void sh1106_draw_line(SH1106_t* dev, int x1, int y1, int x2, int y2, bool invert);
void sh1106_draw_hline(SH1106_t* dev, int x, int y, int width, bool invert);
void sh1106_draw_vline(SH1106_t* dev, int x, int y, int height, bool invert);
void sh1106_draw_rect(SH1106_t* dev, int x, int y, int width, int height, bool invert);
void sh1106_draw_pixel(SH1106_t* dev, int x, int y, bool invert);
void sh1106_draw_custom_box(SH1106_t* dev);

void i2c_master_init(SH1106_t* dev, int16_t sda, int16_t scl, int16_t reset);
void i2c_init(SH1106_t* dev, int width, int height);
void i2c_display_image(SH1106_t* dev, int page, int seg, uint8_t* images, int width);
void i2c_contrast(SH1106_t* dev, int contrast);
void i2c_hardware_scroll(SH1106_t* dev, sh1106_scroll_type_t scroll);

void spi_master_init(SH1106_t* dev, int16_t GPIO_MOSI, int16_t GPIO_SCLK, int16_t GPIO_CS, int16_t GPIO_DC, int16_t GPIO_RESET);
bool spi_master_write_byte(spi_device_handle_t SPIHandle, const uint8_t* Data, size_t DataLength);
bool spi_master_write_command(SH1106_t* dev, uint8_t Command);
bool spi_master_write_data(SH1106_t* dev, const uint8_t* Data, size_t DataLength);
void spi_init(SH1106_t* dev, int width, int height);
void spi_display_image(SH1106_t* dev, int page, int seg, uint8_t* images, int width);
void spi_contrast(SH1106_t* dev, int contrast);
void spi_hardware_scroll(SH1106_t* dev, sh1106_scroll_type_t scroll);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_SH1106_H_ */

#include <stdio.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "font8x8_basic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "sh1106.h"

#define BOOT_BUTTON_PIN GPIO_NUM_9
#define LEFT_BUTTON_PIN GPIO_NUM_22
#define RIGHT_BUTTON_PIN GPIO_NUM_1
#define UP_BUTTON_PIN GPIO_NUM_15
#define DOWN_BUTTON_PIN GPIO_NUM_23
#define BUTTON_ACTIVE_LEVEL 0

#define BOOT_BUTTON_MASK 0b0000 << 4
#define LEFT_BUTTON_MASK 0b0001 << 4
#define RIGHT_BUTTON_MASK 0b0010 << 4
#define UP_BUTTON_MASK 0b0011 << 4
#define DOWN_BUTTON_MASK 0b0100 << 4

#define LOGO_HEIGHT 128
#define LOGO_WIDTH 64
const unsigned char epd_bitmap_logo[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1b, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3c, 0x70, 0x03, 0xf8, 0x18, 0x18, 0xff, 0xc7, 0xfe, 0x3f, 0xf1, 0xfe, 0x00, 0x00,
    0x00, 0x03, 0x73, 0x99, 0x83, 0xf8, 0x18, 0x18, 0xff, 0xc7, 0xfe, 0x3f, 0xf1, 0xfe, 0x00, 0x00,
    0x00, 0x07, 0xe7, 0xcf, 0xc3, 0x18, 0x18, 0x18, 0xc0, 0x06, 0x00, 0x30, 0x31, 0xc1, 0xc0, 0x00,
    0x00, 0x0f, 0xc7, 0xc7, 0xe3, 0x18, 0x18, 0x18, 0xc0, 0x06, 0x00, 0x30, 0x31, 0xc1, 0xc0, 0x00,
    0x00, 0x0e, 0xd7, 0xd6, 0x63, 0x18, 0x18, 0x18, 0xc0, 0x06, 0x00, 0x30, 0x31, 0xc1, 0xc0, 0x00,
    0x00, 0x04, 0xb7, 0xda, 0x03, 0xf8, 0x18, 0x18, 0xc0, 0xc6, 0x00, 0x30, 0x31, 0xc1, 0xc0, 0x00,
    0x00, 0x00, 0x3b, 0xb8, 0x03, 0xf8, 0x18, 0x18, 0xc0, 0xc6, 0x00, 0x30, 0x31, 0xc1, 0xc0, 0x00,
    0x00, 0x00, 0xa4, 0x4a, 0x03, 0x07, 0x18, 0x18, 0xc0, 0xc6, 0x00, 0x30, 0x31, 0xc1, 0xc0, 0x00,
    0x00, 0x00, 0xc7, 0xc3, 0x03, 0x03, 0x18, 0x18, 0xc0, 0xc6, 0x00, 0x30, 0x31, 0xc1, 0xc0, 0x00,
    0x00, 0x01, 0xcf, 0xe7, 0x03, 0x03, 0x18, 0x18, 0xc0, 0xc6, 0x00, 0x30, 0x31, 0xc1, 0xc0, 0x00,
    0x00, 0x03, 0xed, 0x6f, 0x83, 0xff, 0x1f, 0xf8, 0xff, 0xc7, 0xfe, 0x3f, 0xf1, 0xc1, 0xc0, 0x00,
    0x00, 0x07, 0x33, 0x99, 0xc3, 0xff, 0x1f, 0xf8, 0xff, 0xc7, 0xfe, 0x3f, 0xf1, 0xc1, 0xc0, 0x00,
    0x00, 0x06, 0x1c, 0x70, 0xc3, 0xff, 0x1f, 0xf8, 0xff, 0xc7, 0xfe, 0x3f, 0xf1, 0xc1, 0xc0, 0x00,
    0x00, 0x00, 0x0f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xbe, 0xf7, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa2, 0x10, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xaa, 0xf1, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x22, 0x80, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xbe, 0xf7, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const char* TAG = "main";
SH1106_t dev;

const char* button_event_table[] = {
    "BUTTON_PRESS_DOWN",
    "BUTTON_PRESS_UP",
    "BUTTON_PRESS_REPEAT",
    "BUTTON_PRESS_REPEAT_DONE",
    "BUTTON_SINGLE_CLICK",
    "BUTTON_DOUBLE_CLICK",
    "BUTTON_MULTIPLE_CLICK",
    "BUTTON_LONG_PRESS_START",
    "BUTTON_LONG_PRESS_HOLD",
    "BUTTON_LONG_PRESS_UP",
};

const char* button_name_table[] = {
    "BOOT",
    "LEFT",
    "RIGHT",
    "UP",
    "DOWN",
};

enum button_name {
    BOOT = 0,
    LEFT,
    RIGHT,
    UP,
    DOWN,
};

static void button_event_cb(void* arg, void* data) {
    uint8_t button_name = (((button_event_t)data) >> 4); // >> 4 to get the button number
    uint8_t button_event = ((button_event_t)data) & 0x0F; // & 0x0F to get the event number without the mask
    const char* button_name_str = button_name_table[button_name];
    const char* button_event_str = button_event_table[button_event];
    ESP_LOGI(TAG, "Button: %s, Event: %s", button_name_str, button_event_str);

    // Show button name (from table) in the display
    sh1106_clear_screen(&dev, false);
    switch(button_name) {
        case BOOT:
            sh1106_display_text(&dev, 0, "BOOT", 4, false);
            break;
        case LEFT:
            sh1106_display_text(&dev, 0, "LEFT", 4, false);
            break;
        case RIGHT:
            sh1106_display_text(&dev, 0, "RIGHT", 5, false);
            break;
        case UP:
            sh1106_display_text(&dev, 0, "UP", 2, false);
            break;
        case DOWN:
            sh1106_display_text(&dev, 0, "DOWN", 4, false);
            break;
    }

    // Draw a rectangle to embbed the text
    sh1106_draw_rect(&dev, 0, 0, 126, 8, 0);
}

void button_init(uint32_t button_num, uint8_t mask) {
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = button_num,
            .active_level = BUTTON_ACTIVE_LEVEL,
        },
    };
    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn);
    esp_err_t err = iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_event_cb, (void*)(BUTTON_PRESS_DOWN | mask));
    err |= iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb, (void*)(BUTTON_PRESS_UP | mask));
    err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb, (void*)(BUTTON_PRESS_REPEAT | mask));
    err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb, (void*)(BUTTON_PRESS_REPEAT_DONE | mask));
    err |= iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, (void*)(BUTTON_SINGLE_CLICK | mask));
    err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb, (void*)(BUTTON_DOUBLE_CLICK | mask));
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb, (void*)(BUTTON_LONG_PRESS_START | mask));
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb, (void*)(BUTTON_LONG_PRESS_HOLD | mask));
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb, (void*)(BUTTON_LONG_PRESS_UP | mask));
    ESP_ERROR_CHECK(err);
}

void hello_task(void* pvParameter) {
    while (1) {
        // printf("Hello world from task!\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    button_init(BOOT_BUTTON_PIN, BOOT_BUTTON_MASK);
    button_init(LEFT_BUTTON_PIN, LEFT_BUTTON_MASK);
    button_init(RIGHT_BUTTON_PIN, RIGHT_BUTTON_MASK);
    button_init(UP_BUTTON_PIN, UP_BUTTON_MASK);
    button_init(DOWN_BUTTON_PIN, DOWN_BUTTON_MASK);
    xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
    printf("Hello world!\n");

    int center = 3, top, bottom;
    char lineChar[20];

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
    spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
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

    sh1106_draw_line(&dev, 0, 8, 127, 8, 0);
    sh1106_draw_rect(&dev, 0, 0, 126, 63, 0);
    // Draw a 8x8 box in the center of the screen
    sh1106_draw_rect(&dev, 60, 28, 8, 8, 0);
    // Show logo
    // sh1106_bitmaps(&dev, 0, 0, (uint8_t*)epd_bitmap_logo, LOGO_WIDTH, LOGO_HEIGHT, false);
    // sh1106_bitmaps(&dev, 0, 0, (uint8_t*)epd_bitmap_logo, 128, 32, false);

//     sh1106_clear_screen(&dev, false);
//     sh1106_contrast(&dev, 0xff);
//     sh1106_display_text_x3(&dev, 0, "Hello", 5, false);
//     vTaskDelay(3000 / portTICK_PERIOD_MS);

// #if CONFIG_SH1106_128x64
//     top = 2;
//     center = 3;
//     bottom = 8;
//     sh1106_display_text(&dev, 0, "SH1106 128x64", 14, false);
//     sh1106_display_text(&dev, 1, "ABCDEFGHIJKLMNOP", 16, false);
//     sh1106_display_text(&dev, 2, "abcdefghijklmnop", 16, false);
//     sh1106_display_text(&dev, 3, "Hello World!!", 13, false);
//     // sh1106_clear_line(&dev, 4, true);
//     // sh1106_clear_line(&dev, 5, true);
//     // sh1106_clear_line(&dev, 6, true);
//     // sh1106_clear_line(&dev, 7, true);
//     sh1106_display_text(&dev, 4, "SH1106 128x64", 14, true);
//     sh1106_display_text(&dev, 5, "ABCDEFGHIJKLMNOP", 16, true);
//     sh1106_display_text(&dev, 6, "abcdefghijklmnop", 16, true);
//     sh1106_display_text(&dev, 7, "Hello World!!", 13, true);
// #endif  // CONFIG_SH1106_128x64

// #if CONFIG_SH1106_128x32
//     top = 1;
//     center = 1;
//     bottom = 4;
//     sh1106_display_text(&dev, 0, "SH1106 128x32", 14, false);
//     sh1106_display_text(&dev, 1, "Hello World!!", 13, false);
//     // sh1106_clear_line(&dev, 2, true);
//     // sh1106_clear_line(&dev, 3, true);
//     sh1106_display_text(&dev, 2, "SH1106 128x32", 14, true);
//     sh1106_display_text(&dev, 3, "Hello World!!", 13, true);
// #endif  // CONFIG_SH1106_128x32
//     vTaskDelay(3000 / portTICK_PERIOD_MS);

//     // Invert
//     sh1106_clear_screen(&dev, true);
//     sh1106_contrast(&dev, 0xff);
//     sh1106_display_text(&dev, center, "  Good Bye!!", 12, true);
//     vTaskDelay(5000 / portTICK_PERIOD_MS);

//     // Fade Out
//     sh1106_fadeout(&dev);

#if 0
	// Fade Out
	for(int contrast=0xff;contrast>0;contrast=contrast-0x20) {
		sh1106_contrast(&dev, contrast);
		vTaskDelay(40);
	}
#endif

    // Restart module
    // esp_restart();
}

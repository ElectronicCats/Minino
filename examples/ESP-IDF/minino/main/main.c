#include <stdio.h>
#include "button_helper.h"
#include "display.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "font8x8_basic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"

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

static const char* TAG = "main";

static void button_event_cb(void* arg, void* data) {
    uint8_t button_name = (((button_event_t)data) >> 4);   // >> 4 to get the button number
    uint8_t button_event = ((button_event_t)data) & 0x0F;  // & 0x0F to get the event number without the mask
    const char* button_name_str = button_name_table[button_name];
    const char* button_event_str = button_event_table[button_event];
    ESP_LOGI(TAG, "Button: %s, Event: %s", button_name_str, button_event_str);

    if (button_event != BUTTON_PRESS_DOWN)
        return;

    display_menu(button_name, button_event);
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
    display_init();
    xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
    printf("Hello world!\n");
}

#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"

#define BOOT_BUTTON_PIN GPIO_NUM_9
#define LEFT_BUTTON_PIN GPIO_NUM_1
#define RIGHT_BUTTON_PIN GPIO_NUM_22
#define UP_BUTTON_PIN GPIO_NUM_15
#define DOWN_BUTTON_PIN GPIO_NUM_23
#define BUTTON_ACTIVE_LEVEL 0

#define BOOT_BUTTON_MASK 0b0001 << 4
#define LEFT_BUTTON_MASK 0b0010 << 4
#define RIGHT_BUTTON_MASK 0b0011 << 4
#define UP_BUTTON_MASK 0b0100 << 4
#define DOWN_BUTTON_MASK 0b0101 << 4

static const char* TAG = "main";

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

static void button_event_cb(void* arg, void* data) {
	ESP_LOGI(TAG, "Button: %s", button_name_table[(((button_event_t)data) >> 4) - 1]); // >> 4 to get the button number and - 1 to start from 0
    ESP_LOGI(TAG, "Button event %s", button_event_table[((button_event_t)data) & 0x0F]); // & 0x0F to get the event number without the mask
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
}

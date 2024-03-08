#include "keyboard.h"
#include "button_helper.h"
#include "display.h"
#include "esp_log.h"

static const char* TAG = "keyboard";

static void button_event_cb(void* arg, void* data);
void handle_selected_option(void);
void update_previous_layer(void);
void handle_back(void);

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

void keyboard_init() {
    button_init(BOOT_BUTTON_PIN, BOOT_BUTTON_MASK);
    button_init(LEFT_BUTTON_PIN, LEFT_BUTTON_MASK);
    button_init(RIGHT_BUTTON_PIN, RIGHT_BUTTON_MASK);
    button_init(UP_BUTTON_PIN, UP_BUTTON_MASK);
    button_init(DOWN_BUTTON_PIN, DOWN_BUTTON_MASK);
}

static void button_event_cb(void* arg, void* data) {
    uint8_t button_name = (((button_event_t)data) >> 4);   // >> 4 to get the button number
    uint8_t button_event = ((button_event_t)data) & 0x0F;  // & 0x0F to get the event number without the mask
    const char* button_name_str = button_name_table[button_name];
    const char* button_event_str = button_event_table[button_event];
    ESP_LOGI(TAG, "Button: %s, Event: %s", button_name_str, button_event_str);

    if (button_event != BUTTON_PRESS_DOWN)
        return;

    switch (button_name) {
        case BOOT:
            break;
        case LEFT:
            if (current_layer != LAYER_MAIN_MENU)
                handle_back();
            break;
        case RIGHT:
            if (button_event == BUTTON_PRESS_DOWN)
                handle_selected_option();
            break;
        case UP:
            selected_option = (selected_option == 0) ? 0 : selected_option - 1;
            display_menu();
            break;
        case DOWN:
            selected_option = (selected_option == options_length - 3) ? selected_option : selected_option + 1;
            display_menu();
            break;
    }

    ESP_LOGI(TAG, "Selected option: %d", selected_option);
    ESP_LOGI(TAG, "Options length: %d", options_length);
    update_previous_layer();
}

void handle_selected_option() {
    switch (current_layer) {
        case LAYER_MAIN_MENU:
            switch (selected_option) {
                case MAIN_MENU_APPLICATIONS:
                    current_layer = LAYER_APPLICATIONS;
                    break;
                case MAIN_MENU_SETTINGS:
                    // current_layer = LAYER_SETTINGS;
                    break;
                case MAIN_MENU_ABOUT:
                    // current_layer = LAYER_ABOUT;
                    break;
            }
            break;
        case LAYER_APPLICATIONS:
        case LAYER_SETTINGS:
        case LAYER_ABOUT:
            break;
    }

    display_menu();
}

void update_previous_layer() {
    switch (current_layer) {
        case LAYER_MAIN_MENU:
        case LAYER_APPLICATIONS:
        case LAYER_SETTINGS:
        case LAYER_ABOUT:
            previous_layer = LAYER_MAIN_MENU;
            break;
        default:
            ESP_LOGE(TAG, "Invalid layer");
            break;
    }
}

void handle_back() {
    current_layer = previous_layer;
    selected_option = 0;
    display_menu();
}

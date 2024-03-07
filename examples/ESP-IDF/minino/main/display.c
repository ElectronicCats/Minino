#include "display.h"
#include "button_helper.h"
#include "display_helper.h"
#include "esp_log.h"
#include "string.h"

static const char* TAG = "display";
SH1106_t dev;
uint8_t selected_option;
Layer previous_layer;
Layer current_layer;
int options_length;

void display_init() {
    selected_option = 0;
    previous_layer = LAYER_MAIN_MENU;
    current_layer = LAYER_MAIN_MENU;
    options_length = 0;

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
    sh1106_show_buffer(&dev);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    display_clear();
    display_text("Page: 1", 7, 0, NO_INVERT);
    display_text("Page: 2", 7, 1, NO_INVERT);
    display_text("Page: 3", 7, 2, NO_INVERT);
    display_text("Page: 4", 7, 3, NO_INVERT);
    display_text("Page: 5", 7, 4, NO_INVERT);
    display_text("Page: 6", 7, 5, NO_INVERT);
    display_text("Page: 7", 7, 6, NO_INVERT);
}

void display_clear() {
    sh1106_clear_screen(&dev, false);
}

/// @brief Display text on the screen
/// @param text 
/// @param text_size 
/// @param page 
/// @param invert 
void display_text(const char* text, int text_size, int page, int invert) {
    sh1106_display_text(&dev, page, text, text_size, invert);
}

/// @brief Display a box around the selected item
void display_selected_item_box() {
    sh1106_draw_custom_box(&dev);
}

/// @brief Add empty strings at the beginning and end of the array
/// @param array 
/// @param length 
/// @return 
char** add_empty_strings(char** array, int length) {
    char** newArray = malloc((length + 2) * sizeof(char*));

    // Add the empty string at the beginning
    newArray[0] = strdup("");

    // Copy the original array
    for (int i = 0; i < length; i++) {
        newArray[i + 1] = strdup(array[i]);
    }

    // Add the empty string at the end
    newArray[length + 1] = strdup("");

    options_length = length + 2;

    return newArray;
}

char** get_menu_items() {
    switch (current_layer) {
        case LAYER_MAIN_MENU:
            options_length = sizeof(mainOptions) / sizeof(mainOptions[0]);
            return add_empty_strings(mainOptions, options_length);
        // case SETTINGS:
        // return settingsOptions;
        // case ABOUT:
        // return aboutOptions;
        default:
            ESP_LOGE(TAG, "Invalid layer");
            return NULL;
    }
}

void display_menu(uint8_t button_name, uint8_t button_event) {
    char** options = get_menu_items();

    switch (button_name) {
        case BOOT:
            break;
        case LEFT:
            break;
        case RIGHT:
            break;
        case UP:
            selected_option = (selected_option == 0) ? 0 : selected_option - 1;
            break;
        case DOWN:
            selected_option = (selected_option == options_length - 3) ? selected_option : selected_option + 1;
            break;
    }

    ESP_LOGI(TAG, "Selected option: %d", selected_option);
    ESP_LOGI(TAG, "Options length: %d", options_length);

    // Show only 3 options at a time in the following order:
    // Page 1: Option 1
    // Page 3: Option 2 -> selected option
    // Page 5: Option 3

    display_clear();
    int page = 1;
    for (int i = 0; i < 3; i++) {
        char* text = (char*)malloc(16);
        if (i == 0) {
            sprintf(text, " %s", options[i + selected_option]);
        } else if (i == 1) {
            sprintf(text, "  %s", options[i + selected_option]);
        } else {
            sprintf(text, " %s", options[i + selected_option]);
        }

        display_text(text, strlen(text), page, NO_INVERT);
        page += 2;
    }

    display_selected_item_box();
}

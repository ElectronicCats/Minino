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

    wifi_sniffer_register_cb(display_wifi_sniffer);

    // Show logo
    display_clear();
    // buzzer_play();
    sh1106_bitmaps(&dev, 0, 0, epd_bitmap_logo_1, 128, 64, NO_INVERT);
    // buzzer_stop();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    vTaskDelay(900 / portTICK_PERIOD_MS);
    display_menu();
}

void display_clear() {
    sh1106_clear_screen(&dev, false);
}

void display_show() {
    sh1106_show_buffer(&dev);
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
            options_length = sizeof(main_options) / sizeof(main_options[0]);
            return add_empty_strings(main_options, options_length);
        case LAYER_SETTINGS:
            options_length = sizeof(settings_options) / sizeof(settings_options[0]);
            return add_empty_strings(settings_options, options_length);
        // case LAYER_ABOUT:
        // return about_options;
        case LAYER_APPLICATIONS:
            options_length = sizeof(applications_options) / sizeof(applications_options[0]);
            return add_empty_strings(applications_options, options_length);
        case LAYER_WIFI_APPS:
            options_length = sizeof(wifi_options) / sizeof(wifi_options[0]);
            return add_empty_strings(wifi_options, options_length);
        case LAYER_WIFI_ANALIZER:
            options_length = 0;
            return NULL;
        case LAYER_THREAD_APPS:
            options_length = sizeof(thread_options) / sizeof(thread_options[0]);
            return add_empty_strings(thread_options, options_length);
        case LAYER_THREAD_CLI:
            options_length = 0;
            return NULL;
        default:
            ESP_LOGE(TAG, "Invalid layer");
            return NULL;
    }
}

void display_menu() {
    char** options = get_menu_items();

    if (options == NULL) {
        ESP_LOGE(TAG, "Options is NULL");
        return;
    }

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

void display_wifi_sniffer(wifi_sniffer_record_t record) {
    char* channel_str = (char*)malloc(16);
    char* ssid_str = (char*)malloc(50);
    char* rssi_str = (char*)malloc(16);
    char* addr_str = (char*)malloc(16);
    char* hash_str = (char*)malloc(16);
    char* htci_str = (char*)malloc(16);
    char* sn_str = (char*)malloc(16);
    char* time_str = (char*)malloc(16);

    sprintf(channel_str, "Channel=%d", record.channel);
    sprintf(ssid_str, "SSID=%s", record.ssid);
    // sprintf(addr_str, "ADDR=%02x:%02x:%02x:%02x:%02x:%02x", record.addr[0], record.addr[1], record.addr[2], record.addr[3], record.addr[4], record.addr[5]);
    sprintf(hash_str, "Hash=%s", record.hash);
    sprintf(rssi_str, "RSSI=%d", record.rssi);
    sprintf(htci_str, "HTCI=%s", record.htci);
    sprintf(sn_str, "SN=%d", record.sn);
    sprintf(time_str, "Time=%d", (int)record.timestamp);

    display_clear();
    display_text(channel_str, 16, 0, NO_INVERT);
    display_text(ssid_str, 16, 1, NO_INVERT);
    // display_text(addr_str, 16, 2, NO_INVERT);
    display_text(hash_str, 16, 3, NO_INVERT);
    display_text(rssi_str, 16, 4, NO_INVERT);
    display_text(htci_str, 16, 5, NO_INVERT);
    display_text(sn_str, 16, 6, NO_INVERT);
    display_text(time_str, 16, 7, NO_INVERT);

    ESP_LOGI(TAG,
             "ADDR=%02x:%02x:%02x:%02x:%02x:%02x, "
             "SSID=%s, "
             "TIMESTAMP=%d, "
             "HASH=%s, "
             "RSSI=%02d, "
             "SN=%d, "
             "HT CAP. INFO=%s",
             record.addr[0], record.addr[1], record.addr[2], record.addr[3], record.addr[4],
             record.addr[5], record.ssid, (int)record.timestamp, record.hash, record.rssi, record.sn, record.htci);
}

void display_thread_cli() {
    display_clear();
    display_text("Thread CLI      ", 16, 0, INVERT);
    display_text("Connect Minino", 16, 1, NO_INVERT);
    display_text("to a computer", 16, 2, NO_INVERT);
    display_text("via USB and use", 16, 3, NO_INVERT);
    display_text("screen command", 16, 4, NO_INVERT);
    display_text("(linux or mac)", 16, 5, NO_INVERT);
    display_text("or putty in", 16, 6, NO_INVERT);
    display_text("windows", 16, 7, NO_INVERT);
    display_show();
}

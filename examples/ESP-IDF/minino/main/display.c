#include "display.h"
#include "button_helper.h"
#include "display_helper.h"
#include "esp_log.h"
#include "string.h"
#include "thread_cli.h"

static const char* TAG = "display";
SH1106_t dev;
uint8_t selected_option;
Layer previous_layer;
Layer current_layer;
int num_items;
uint8_t bluetooth_device_count;

void display_init() {
    selected_option = 0;
    previous_layer = LAYER_MAIN_MENU;
    current_layer = LAYER_MAIN_MENU;
    num_items = 0;
    bluetooth_device_count = 0;

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
    bluetooth_scanner_register_cb(display_bluetooth_scanner);

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
void display_text(const char* text, int x, int page, int invert) {
    sh1106_display_text(&dev, page, text, x, invert);
}

/// @brief Clear a line on the screen
/// @param page
/// @param invert
void display_clear_line(int page, int invert) {
    sh1106_clear_line(&dev, page, invert);
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

    num_items = length + 2;

    return newArray;
}

char** get_menu_items() {
    num_items = 0;
    char** submenu = menu_items[current_layer];
    if (submenu != NULL) {
        while (submenu[num_items] != NULL) {
            // ESP_LOGI(TAG, "Item: %s", submenu[num_items]);
            num_items++;
        }
    }
    // ESP_LOGI(TAG, "Number of items: %d", num_items);

    if (num_items == 0) {
        return NULL;
    }

    return add_empty_strings(menu_items[current_layer], num_items);
}

void display_menu() {
    char** items = get_menu_items();

    if (items == NULL) {
        ESP_LOGW(TAG, "Options is NULL");
        return;
    }

    // Show only 3 options at a time in the following order:
    // Page 1: Option 1
    // Page 3: Option 2 -> selected option
    // Page 5: Option 3

    display_clear();
    int page = 1;
    for (int i = 0; i < 3; i++) {
        char* text = (char*)malloc(20);
        if (i == 0) {
            sprintf(text, " %s", items[i + selected_option]);
        } else if (i == 1) {
            sprintf(text, "  %s", items[i + selected_option]);
        } else {
            sprintf(text, " %s", items[i + selected_option]);
        }

        display_text(text, 0, page, NO_INVERT);
        page += 2;
    }

    display_selected_item_box();
}

void display_wifi_sniffer(wifi_sniffer_record_t record) {
    char* channel_str = (char*)malloc(16);
    char* ssid_str = (char*)malloc(50);
    char* rssi_str = (char*)malloc(16);
    char* addr_str = (char*)malloc(30);
    char* hash_str = (char*)malloc(16);
    char* htci_str = (char*)malloc(16);
    char* sn_str = (char*)malloc(16);
    char* time_str = (char*)malloc(16);

    sprintf(channel_str, "Channel=%d", record.channel);
    display_text("WiFi Sniffer    ", 0, 0, INVERT);
    display_clear_line(1, NO_INVERT);
    display_text(channel_str, 0, 1, NO_INVERT);

    if (record.ssid == NULL && record.timestamp == 0) {
        return;
    }

    sprintf(ssid_str, "SSID=%s", record.ssid);
    sprintf(addr_str, "ADDR=%02x:%02x:%02x:%02x:%02x:%02x", record.addr[0], record.addr[1], record.addr[2], record.addr[3], record.addr[4], record.addr[5]);
    sprintf(hash_str, "Hash=%s", record.hash);
    sprintf(rssi_str, "RSSI=%d", record.rssi);
    sprintf(htci_str, "HTCI=%s", record.htci);
    sprintf(sn_str, "SN=%d", record.sn);
    sprintf(time_str, "Time=%d", (int)record.timestamp);

    display_clear();
    display_text("WiFi Sniffer    ", 0, 0, INVERT);
    display_text(ssid_str, 0, 2, NO_INVERT);
    display_text(addr_str, 0, 3, NO_INVERT);
    display_text(hash_str, 0, 4, NO_INVERT);
    display_text(rssi_str, 0, 5, NO_INVERT);
    display_text(htci_str, 0, 7, NO_INVERT);
    // display_text(sn_str, 0, 6, NO_INVERT);
    display_text(time_str, 0, 8, NO_INVERT);

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

void display_bluetooth_scanner(bluetooth_scanner_record_t record) {
    display_text("Airtags Scanner", 0, 0, INVERT);
    display_clear_line(2, NO_INVERT);

    if (!record.is_airtag) {
        bluetooth_device_count++;
        char* device_count_str = (char*)malloc(16);
        sprintf(device_count_str, "Devices=%d", record.count);
        display_text(device_count_str, 0, 2, NO_INVERT);
        return;
    }

    char* name_str = (char*)malloc(50);
    char* addr_str1 = (char*)malloc(14);
    char* addr_str2 = (char*)malloc(14);
    char* rssi_str = (char*)malloc(16);

    sprintf(name_str, "%s", record.name);
    sprintf(addr_str1, "MAC= %02X:%02X:%02X", record.mac[5], record.mac[4], record.mac[3]);
    sprintf(addr_str2, "     %02X:%02X:%02X", record.mac[2], record.mac[1], record.mac[0]);
    sprintf(rssi_str, "RSSI=%d", record.rssi);

    display_text(name_str, 0, 2, NO_INVERT);
    display_text(addr_str1, 0, 3, NO_INVERT);
    display_text(addr_str2, 0, 4, NO_INVERT);
    display_text(rssi_str, 0, 5, NO_INVERT);
    // vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void display_thread_cli() {
    thread_cli_start();

    display_clear();
    display_text("Thread CLI      ", 0, 0, INVERT);
    display_text("Connect Minino", 0, 1, NO_INVERT);
    display_text("to a computer", 0, 2, NO_INVERT);
    display_text("via USB and use", 0, 3, NO_INVERT);
    display_text("screen command", 0, 4, NO_INVERT);
    display_text("(linux or mac)", 0, 5, NO_INVERT);
    display_text("or putty in", 0, 6, NO_INVERT);
    display_text("windows", 0, 7, NO_INVERT);
    display_show();
}

void display_about_info() {
    display_clear();

    switch (selected_option) {
        case ABOUT_MENU_VERSION:
            display_text(" Minino v1.3.0", 0, 3, NO_INVERT);
            display_text("     BETA", 0, 4, NO_INVERT);
            break;
        case ABOUT_MENU_LICENSE:
            display_text("  GNU GPL 3.0", 0, 3, NO_INVERT);
            break;
        case ABOUT_MENU_CREDITS:
            display_text("Developed by", 0, 0, NO_INVERT);
            display_text("Electronic Cats", 0, 1, NO_INVERT);
            display_text("This product is", 0, 3, NO_INVERT);
            display_text("in a BETA stage", 0, 4, NO_INVERT);
            display_text("use at your own", 0, 5, NO_INVERT);
            break;
        case ABOUT_MENU_LEGAL:
            display_text(" In development", 0, 3, NO_INVERT);
            break;
    }
}

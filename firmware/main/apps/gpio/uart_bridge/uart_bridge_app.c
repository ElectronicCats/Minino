#include "uart_bridge_app.h"
#include "cat_console.h"
#include "general_submenu.h"
#include "general_radio_selection.h"
#include "menus_module.h"
#include "uart_bridge.h"
#include "preferences.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char* TAG = "uart_bridge_app";

uint8_t uart_baudrate_option = 1;
uint8_t uart_data_bits_option = 3;
uint8_t uart_parity_option = 0;
uint8_t uart_stop_bits_option = 0;


static const char* uart_bridge_app_options[] = {
  "Start",
  "Baudrate",
  "Data Bits",
  "Parity",
  "Stop Bits",
};

static int uart_baudrate_values[] = {9600, 115200, 230400, 460800, 921600};

static const char* baudrate_options[] = {
  "9600",
  "115200",
  "230400",
  "460800",
  "921600",
};

static const char* data_bits_options[] = {
  "5 bits",
  "6 bits",
  "7 bits",
  "8 bits",
};

static const char* parity_options[] = {
  "disable",
  "odd",
  "even",
};

static const char* stop_bits_options[] = {
  "1 bit",
  "1.5 bits",
  "2 bits",
};

void uart_bridge_app_begin(){
  // uart_config_t uart_config = {0};
  // uart_config.baud_rate = uart_baudrate_values[uart_baudrate_option];
  // uart_config.data_bits = (uart_word_length_t) uart_data_bits_option;
  // uart_config.parity = (uart_parity_t) uart_parity_option;
  // uart_config.stop_bits = (uart_stop_bits_t) uart_stop_bits_option;
  // esp_err_t err = uart_bridge_begin(uart_config, 1024);
  // if (err != ESP_OK) {
  //   ESP_LOGE(TAG, "Failed to begin UART bridge (error code: %d)", err);
  // }


}


void set_uart_baudrate(uint8_t option){
  uart_baudrate_option = option;
}

void set_uart_data_bits(uint8_t option){
  uart_data_bits_option = option;
}

void set_uart_parity(uint8_t option){
  uart_parity_option = option;
}

void set_uart_stop_bits(uint8_t option){
  uart_stop_bits_option = option;
}

// Display menus on uart bridge config for baudrate
void set_uart_baudrate_menu_config(){
  general_radio_selection_menu_t settings = {0};
  settings.banner = "Baudrate";
  settings.options = baudrate_options;
  settings.options_count = sizeof(baudrate_options) / sizeof(char*);
  settings.select_cb = set_uart_baudrate;
  settings.exit_cb = uart_bridge_app_enter;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.current_option = uart_baudrate_option;
  general_radio_selection(settings);

}

// Display menus on uart bridge config for data bits
void set_uart_data_bits_menu_config(){
  general_radio_selection_menu_t settings = {0};
  settings.banner = "Data Bits";
  settings.options = data_bits_options;
  settings.options_count = sizeof(data_bits_options) / sizeof(char*);
  settings.select_cb = set_uart_data_bits;
  settings.exit_cb = uart_bridge_app_enter;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.current_option = uart_data_bits_option;
  general_radio_selection(settings);
}

// Display menus on uart bridge config for parity
void set_uart_parity_menu_config(){
  general_radio_selection_menu_t settings = {0};
  settings.banner = "Parity";
  settings.options = parity_options;
  settings.options_count = sizeof(parity_options) / sizeof(char*);
  settings.select_cb = set_uart_parity;
  settings.exit_cb = uart_bridge_app_enter;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.current_option = uart_parity_option;
  general_radio_selection(settings);
}

// Display menus on uart bridge config for stop bits
void set_uart_stop_bits_menu_config(){
  general_radio_selection_menu_t settings = {0};
  settings.banner = "Stop Bits";
  settings.options = stop_bits_options;
  settings.options_count = sizeof(stop_bits_options) / sizeof(char*);
  settings.select_cb = set_uart_stop_bits;
  settings.exit_cb = uart_bridge_app_enter;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.current_option = uart_stop_bits_option;
  general_radio_selection(settings);
}

void uart_select_option(uint8_t option){
  switch (option) {
    case 0:
      break;
    case 1:
      set_uart_baudrate_menu_config();
      break;
    case 2:
      set_uart_data_bits_menu_config();
      break;
    case 3:
      set_uart_parity_menu_config();
      break;
    case 4:
      set_uart_stop_bits_menu_config();
      break;
    default:
      break;
  }
}

void uart_bridge_app_enter(){
  general_submenu_menu_t menu = {0};
  menu.options = uart_bridge_app_options;
  menu.options_count = sizeof(uart_bridge_app_options) / sizeof(char*);
  menu.select_cb = uart_select_option;
  menu.exit_cb = menus_module_return;
  general_submenu(menu);
}
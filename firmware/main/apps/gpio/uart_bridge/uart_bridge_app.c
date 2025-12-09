#include "uart_bridge_app.h"
#include "cat_console.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "general_radio_selection.h"
#include "general_submenu.h"
#include "keyboard_module.h"
#include "menus_module.h"
#include "oled_driver.h"
#include "preferences.h"
#include "task_manager.h"
#include "uart_bridge.h"
#include "uart_bridge_screen.h"

static const char* TAG = "uart_bridge_app";

static TaskHandle_t uart_bridge_app_task_handle = NULL;

uint8_t uart_baudrate_option = 1;
uint8_t uart_data_bits_option = 3;
uint8_t uart_parity_option = 0;
uint8_t uart_stop_bits_option = 0;

static const char* uart_bridge_app_options[] = {
    "Start", "Baudrate", "Data Bits", "Parity", "Stop Bits",
};

static int uart_baudrate_values[] = {9600, 115200, 230400, 460800, 921600};

static const char* baudrate_options[] = {
    "9600", "115200", "230400", "460800", "921600",
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

static volatile bool uart_bridge_exit = false;

void uart_bridge_app_task(void* args) {
  display_uart_bridge_screen();

  uart_bridge_exit = false;

  cat_console_pause();

  usb_serial_jtag_driver_config_t config = {
      .tx_buffer_size = 1024,
      .rx_buffer_size = 1024,
  };

  // Add this part to check if the driver is installed
  // Here on Minino the driver is installed by default
  // on the command line or cat_console
  if (!usb_serial_jtag_is_driver_installed())
    usb_serial_jtag_driver_install(&config);

  uart_config_t uart_config = {0};
  uart_config.baud_rate = uart_baudrate_values[uart_baudrate_option];
  uart_config.data_bits = (uart_word_length_t) uart_data_bits_option;
  uart_config.parity = (uart_parity_t) uart_parity_option;
  uart_config.stop_bits = (uart_stop_bits_t) uart_stop_bits_option;

  uart_bridge_begin(uart_config, 1024);

  char buffer[1024];

  while (!uart_bridge_exit) {
    int bytes_from_usb =
        usb_serial_jtag_read_bytes(buffer, sizeof(buffer), pdMS_TO_TICKS(10));
    if (bytes_from_usb > 0) {
      uart_bridge_write(buffer, bytes_from_usb);
    }

    int bytes_from_uart =
        uart_read_bytes(UART_NUM_0, buffer, sizeof(buffer), pdMS_TO_TICKS(10));
    if (bytes_from_uart > 0) {
      usb_serial_jtag_write_bytes(buffer, bytes_from_uart, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  uart_bridge_end();
  cat_console_resume();

  vTaskDelete(NULL);
}

static void keyboard_navigation_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN)
    return;

  if (button_name == BUTTON_LEFT) {
    uart_bridge_exit = true;
    uart_bridge_app_enter();
  }
}

void uart_bridge_app_begin() {
  uart_bridge_exit = false;

  esp_err_t err = task_manager_create(
      uart_bridge_app_task, "uart_bridge_app_task", TASK_STACK_MEDIUM, NULL,
      TASK_PRIORITY_LOW, &uart_bridge_app_task_handle);
}

void set_uart_baudrate(uint8_t option) {
  uart_baudrate_option = option;
}

void set_uart_data_bits(uint8_t option) {
  uart_data_bits_option = option;
}

void set_uart_parity(uint8_t option) {
  uart_parity_option = option;
}

void set_uart_stop_bits(uint8_t option) {
  uart_stop_bits_option = option;
}

// Display menus on uart bridge config for baudrate
void set_uart_baudrate_menu_config() {
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
void set_uart_data_bits_menu_config() {
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
void set_uart_parity_menu_config() {
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
void set_uart_stop_bits_menu_config() {
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

void uart_select_option(uint8_t option) {
  switch (option) {
    case 0:
      menus_module_set_app_state(true, keyboard_navigation_cb);
      uart_bridge_app_begin();
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

void uart_bridge_app_enter() {
  general_submenu_menu_t menu = {0};
  menu.options = uart_bridge_app_options;
  menu.options_count = sizeof(uart_bridge_app_options) / sizeof(char*);
  menu.select_cb = uart_select_option;
  menu.exit_cb = menus_module_return;
  general_submenu(menu);
}
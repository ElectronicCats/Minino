#include "logs_output.h"
#include "esp_log.h"
#include "general_radio_selection.h"
#include "menus_module.h"
#include "preferences.h"
#include "uart_bridge.h"

static char* logs_output_options[] = {
    "USB",
    "UART TXD/RXD",
};

uint8_t logs_output_get_option() {
  return preferences_get_uchar("logs_output", USB);
}

void logs_output_set_output(logs_output_option_t selected_option) {
  switch (selected_option) {
    case USB:
      uart_bridge_set_logs_to_usb();
      preferences_put_uchar("logs_output", USB);
      break;
    case UART:
      uart_bridge_set_logs_to_uart();
      preferences_put_uchar("logs_output", UART);
      break;
    default:
      break;
  }
}

void logs_output() {
  general_radio_selection_menu_t logs_output_menu = {
      .options = logs_output_options,
      .banner = "Logs Output",
      .current_option = logs_output_get_option(),
      .options_count = sizeof(logs_output_options) / sizeof(char*),
      .select_cb = logs_output_set_output,
      .exit_cb = menus_module_exit_app,
      .style = RADIO_SELECTION_OLD_STYLE,
  };

  general_radio_selection(logs_output_menu);
}

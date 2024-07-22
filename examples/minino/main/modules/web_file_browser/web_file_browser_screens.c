#include "web_file_browser_screens.h"
#include "oled_screen.h"

static void show_ready() {
  oled_screen_clear();
  oled_screen_display_text("SSID: MININO_AP", 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("PASS: Cats1234", 0, 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("SERVER:", 0, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("-> 192.168.0.1", 0, 3, OLED_DISPLAY_NORMAL);
}

static void show_error() {
  oled_screen_clear();
  oled_screen_display_text("     Server     ", 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("  Mount Failed  ", 0, 4, OLED_DISPLAY_NORMAL);
}

static void transfer_init(char* file_name) {
  oled_screen_clear_line(0, 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Sending File....", 0, 5, OLED_DISPLAY_NORMAL);
  oled_screen_clear_line(0, 6, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(file_name, 0, 6, OLED_DISPLAY_NORMAL);
}

static void transfer_state(uint8_t* status) {
  static uint8_t div = 0;
  div++;
  if (div != 0) {
    return;
  }
  oled_screen_display_loading_bar(*status, 7);
}

static void transfer_result(bool result) {
  oled_screen_clear_line(0, 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(" File Transfer  ", 0, 6, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(result ? "     Succed     " : "     Failed     ", 0,
                           7, OLED_DISPLAY_NORMAL);
}

void web_file_browse_show_event_handler(uint8_t event, void* context) {
  switch (event) {
    case WEB_FILE_BROWSER_READY_EV:
      show_ready();
      break;
    case WEB_FILE_BROWSER_ALREADY_EV:
      break;
    case WEB_FILE_BROWSER_ERROR_EV:
      show_error();
      break;
    case WEB_FILE_BROWSER_STOP_EV:
      break;
    case WEB_FILE_BROWSER_TRANSFER_INIT_EV:
      transfer_init(context);
      break;
    case WEB_FILE_BROWSER_TRANSFER_STATE_EV:
      transfer_state(context);
      break;
    case WEB_FILE_BROWSER_TRANSFERING_FILE_RESULT_EV:
      transfer_result(context);
      break;
    default:
      break;
  }
}

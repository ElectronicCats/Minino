// Description: Template for creating a submenu.
//

#include "general_submenu.h"

static uint8_t last_submenu_selection =
    0;  // Varaible to store the last selected option

typedef enum {
  SUBMENU_OPTION_1,
  SUBMENU_OPTION_2,  // Add more options here
} submenu_options_t;

static const char* submenu_options[] = {
    "Option 1",
    "Option 2",  // Add more options here
};

static void submenu_handler(uint8_t option) {
  last_submenu_selection = option;
  switch (option) {
    case SUBMENU_OPTION_1:
      // Add your code here
      break;
    case SUBMENU_OPTION_2:
      // Add your code here
      break;
    default:
      break;
  }
}

static void submenu_exit_cb() {
  // Add your code here
}

void submenu_template() {
  general_submenu_menu_t submenu = {0};
  submenu.options = submenu_options;
  submenu.options_count = sizeof(submenu_options) / sizeof(char*);
  submenu.select_cb = submenu_handler;
  submenu.selected_option = last_submenu_selection;
  submenu.exit_cb = submenu_exit_cb;

  general_submenu(submenu);
}
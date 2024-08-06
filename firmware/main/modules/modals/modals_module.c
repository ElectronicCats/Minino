#include "modals_module.h"

#include <string.h>
#include "menu_screens_modules.h"
#include "modals_screens.h"

modal_get_user_selection_t* modal_get_user_selection_ctx;

char* yes_no_options[] = {"NO", "YES", NULL};

void (*custom_list_options_cb)(modal_get_user_selection_t*) = NULL;

static void list_options() {
  if (custom_list_options_cb) {
    custom_list_options_cb(modal_get_user_selection_ctx);
    return;
  }
  modals_screens_default_list_options_cb(modal_get_user_selection_ctx);
}

static void get_user_selection_input_cb(uint8_t button_name,
                                        uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      modal_get_user_selection_ctx->selected_option = -1;
      modal_get_user_selection_ctx->consumed = true;
      break;
    case BUTTON_RIGHT:
      modal_get_user_selection_ctx->consumed = true;
      break;
    case BUTTON_UP:
      modal_get_user_selection_ctx->selected_option =
          modal_get_user_selection_ctx->selected_option == 0
              ? modal_get_user_selection_ctx->options_count - 1
              : modal_get_user_selection_ctx->selected_option - 1;
      list_options();
      break;
    case BUTTON_DOWN:
      modal_get_user_selection_ctx->selected_option =
          ++modal_get_user_selection_ctx->selected_option <
                  modal_get_user_selection_ctx->options_count
              ? modal_get_user_selection_ctx->selected_option
              : 0;
      list_options();
      break;
    default:
      break;
  }
}

int count_items(char** items) {
  int count = 0;
  while (items[count] != NULL) {
    count++;
  }
  return count;
}

int8_t modals_module_get_user_selection(char** options, char* banner) {
  modal_get_user_selection_ctx = malloc(sizeof(modal_get_user_selection_t));
  memset(modal_get_user_selection_ctx, 0, sizeof(modal_get_user_selection_t));
  modal_get_user_selection_ctx->options = options;
  modal_get_user_selection_ctx->options_count = count_items(options);
  modal_get_user_selection_ctx->banner = banner;
  menu_screens_set_app_state(true, get_user_selection_input_cb);
  list_options();
  while (!modal_get_user_selection_ctx->consumed)
    ;
  int8_t selection = modal_get_user_selection_ctx->selected_option;
  free(modal_get_user_selection_ctx);
  return selection;
}
bool modals_module_get_user_y_n_selection(char* banner) {
  modal_get_user_selection_ctx = malloc(sizeof(modal_get_user_selection_t));
  memset(modal_get_user_selection_ctx, 0, sizeof(modal_get_user_selection_t));
  modal_get_user_selection_ctx->options = yes_no_options;
  modal_get_user_selection_ctx->options_count = 2;
  modal_get_user_selection_ctx->banner = banner;
  custom_list_options_cb = modals_screens_list_y_n_options_cb;
  menu_screens_set_app_state(true, get_user_selection_input_cb);
  list_options();
  while (!modal_get_user_selection_ctx->consumed)
    ;
  custom_list_options_cb = NULL;
  bool selection = modal_get_user_selection_ctx->selected_option;
  free(modal_get_user_selection_ctx);
  return selection;
}

void modals_module_show_info(char* head, char* body, size_t time_ms) {
  modals_screens_show_info(head, body, time_ms);
}
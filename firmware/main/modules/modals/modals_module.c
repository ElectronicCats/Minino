#include "modals_module.h"

#include <string.h>
#include "menu_screens_modules.h"
#include "modals_screens.h"

modal_get_user_selection_t* modal_get_user_selection_ctx;

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
      modals_screens_update_options_list(modal_get_user_selection_ctx);
      break;
    case BUTTON_DOWN:
      modal_get_user_selection_ctx->selected_option =
          ++modal_get_user_selection_ctx->selected_option <
                  modal_get_user_selection_ctx->options_count
              ? modal_get_user_selection_ctx->selected_option
              : 0;
      modals_screens_update_options_list(modal_get_user_selection_ctx);
      break;
    default:
      break;
  }
}

int8_t modal_module_get_user_selection(uint8_t options_count, char** options) {
  modal_get_user_selection_ctx = malloc(sizeof(modal_get_user_selection_t));
  memset(modal_get_user_selection_ctx, 0, sizeof(modal_get_user_selection_t));
  modal_get_user_selection_ctx->options = options;
  modal_get_user_selection_ctx->options_count = options_count;
  menu_screens_set_app_state(true, get_user_selection_input_cb);
  modals_screens_update_options_list(modal_get_user_selection_ctx);
  while (!modal_get_user_selection_ctx->consumed)
    ;
  int8_t selection = modal_get_user_selection_ctx->selected_option;
  free(modal_get_user_selection_ctx);
  return selection;
}

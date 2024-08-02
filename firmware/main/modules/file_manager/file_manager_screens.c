#include "file_manager_screens.h"
#include "oled_screen.h"

#define MAX_ITEMS_NUM 7

static void update_list(file_manager_context_t* ctx) {
  oled_screen_clear();
  oled_screen_display_text(ctx->is_root ? "< Exit" : "< Back", 0, 0,
                           OLED_DISPLAY_NORMAL);
  for (uint8_t i = 0; i < MIN(ctx->items_count, MAX_ITEMS_NUM); i++) {
    char* str = (char*) malloc(30);
    sprintf(str, "%s%s", ctx->file_items_arr[i]->name,
            ctx->file_items_arr[i]->is_dir ? ">" : "");
    oled_screen_display_text(str, 0, i + 1, ctx->selected_item == i);
    free(str);
  }
}

static void show_fatal_error(char* error_tag) {}
void file_manager_screens_event_handler(file_manager_events_t event,
                                        void* context) {
  switch (event) {
    case FILE_MANAGER_UPDATE_LIST_EV:
      update_list(context);
      break;
    case FILE_MANAGER_SHOW_FATAL_ERR_EV:
      show_fatal_error(context);
      break;
    default:
      break;
  }
}
#include "file_manager_module.h"

#include <string.h>
#include "dirent.h"

#include "coroutine.h"
#include "file_manager_screens.h"
#include "menu_screens_modules.h"
#include "modals_module.h"
#include "sd_card.h"

#define SD_CARD_ROOT "/sdcard"
#define TAG          "File Manager"

file_manager_context_t* fm_ctx;
file_manager_show_event_cb_t file_manager_show_event_cb = NULL;

typedef enum { CANCELED = -1, RENAME_OPTION, ERASE_OPTION } file_options_t;

char* file_options[] = {"Rename", "Erase"};

static void file_manager_input_cb(uint8_t button_name, uint8_t button_event);

void file_manager_set_show_event_callback(file_manager_show_event_cb_t cb) {
  file_manager_show_event_cb = cb;
}

static void show_event(file_manager_events_t event, void* context) {
  if (file_manager_show_event_cb) {
    file_manager_show_event_cb(event, context);
  }
}

static void clear_items() {
  fm_ctx->items_count = 0;
  for (uint8_t i = 0; i < fm_ctx->items_count; i++) {
    free(fm_ctx->file_items_arr[i]);
  }
  free(fm_ctx->file_items_arr);
}

static void get_parent_path(const char* path, char* parent_path) {
  char temp_path[256];
  strncpy(temp_path, path, sizeof(temp_path));
  temp_path[sizeof(temp_path) - 1] = '\0';

  char* last_slash = strrchr(temp_path, '/');
  if (last_slash != NULL) {
    if (last_slash == temp_path) {
      strcpy(parent_path, "/");
    } else {
      size_t len = last_slash - temp_path;
      strncpy(parent_path, temp_path, len);
      parent_path[len] = '\0';
    }
  } else {
    strcpy(parent_path, ".");
  }
}

static void update_files() {
  clear_items();

  DIR* dir;
  struct dirent* entry;
  dir = opendir(fm_ctx->current_path);  // check false
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
      fm_ctx->items_count++;
    }
  }
  closedir(dir);

  fm_ctx->is_root = strcmp(SD_CARD_ROOT, fm_ctx->current_path) == 0;

  fm_ctx->file_items_arr =
      malloc(fm_ctx->items_count * sizeof(file_item_t*));  // check false

  dir = opendir(fm_ctx->current_path);  // check false

  uint16_t idx = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
      file_item_t* item = malloc(sizeof(file_item_t*));  // check false
      item->is_dir = (entry->d_type == DT_DIR);
      item->name = strdup(entry->d_name);  // check false
      size_t path_len =
          strlen(fm_ctx->current_path) + strlen(entry->d_name) + 2;
      item->path = malloc(path_len);  // chek false
      snprintf(item->path, path_len, "%s/%s", fm_ctx->current_path,
               entry->d_name);
      fm_ctx->file_items_arr[idx++] = item;
    }
  }
  closedir(dir);
}

static void print_files() {
  show_event(FILE_MANAGER_UPDATE_LIST_EV, fm_ctx);
}

static void refresh_files() {
  update_files();
  print_files();
}

static file_manager_context_t* file_manager_context_alloc() {
  file_manager_context_t* ctx =
      malloc(sizeof(file_manager_context_t));  // check false
  memset(ctx, 0, sizeof(file_manager_context_t));
  ctx->current_path = SD_CARD_ROOT;
  ctx->file_items_arr = NULL;
  return ctx;
}

static void file_manager_module_exit() {
  clear_items();
  free(fm_ctx);
  menu_screens_exit_submenu();
  menu_screens_set_app_state(false, NULL);
}

static void navigation_up() {
  fm_ctx->selected_item = fm_ctx->selected_item == 0
                              ? fm_ctx->items_count - 1
                              : fm_ctx->selected_item - 1;
  print_files();
}
static void navigation_down() {
  fm_ctx->selected_item =
      ++fm_ctx->selected_item < fm_ctx->items_count ? fm_ctx->selected_item : 0;
  print_files();
}

static void navigation_back() {
  if (fm_ctx->is_root) {
    file_manager_module_exit();
  } else {
    get_parent_path(fm_ctx->current_path, fm_ctx->current_path);
    fm_ctx->selected_item = 0;
    refresh_files();
  }
}

static void file_options_handler(int8_t selection) {
  switch (selection) {
    case RENAME_OPTION:

      break;
    case ERASE_OPTION:
      if (remove(fm_ctx->file_items_arr[fm_ctx->selected_item]->path) == 0) {
        printf(
            "Removed\n");  /////////////////////////////////////////////////////
      } else {
        printf(
            "Failed to Remove\n");  /////////////////////////////////////////////////////
      }
      break;

    default:
      break;
  }
}

static void open_file_options() {
  int8_t selection = modal_module_get_user_selection(2, file_options);
  menu_screens_set_app_state(true, file_manager_input_cb);
  file_options_handler(selection);
  update_files();
  fm_ctx->selected_item = MIN(fm_ctx->selected_item, fm_ctx->items_count - 1);
  print_files();
  vTaskDelete(NULL);
}

static void navigation_enter() {
  if (fm_ctx->file_items_arr[fm_ctx->selected_item]->is_dir) {
    fm_ctx->current_path = fm_ctx->file_items_arr[fm_ctx->selected_item]->path;
    fm_ctx->selected_item = 0;
    refresh_files();
  } else {
    start_coroutine(open_file_options, NULL);
  }
}

static void file_manager_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      navigation_back();
      break;
    case BUTTON_RIGHT:
      navigation_enter();
      break;
    case BUTTON_UP:
      navigation_up();
      break;
    case BUTTON_DOWN:
      navigation_down();
      break;
    default:
      break;
  }
}

void file_manager_module_init() {
  menu_screens_set_app_state(true, file_manager_input_cb);
  if (sd_card_mount() != ESP_OK)
    return;  // check false
  fm_ctx = file_manager_context_alloc();
  file_manager_set_show_event_callback(file_manager_screens_event_handler);
  refresh_files();
}
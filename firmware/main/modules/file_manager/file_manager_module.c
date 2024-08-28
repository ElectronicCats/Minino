#include "file_manager_module.h"

#include <errno.h>
#include <string.h>
#include "dirent.h"
#include "esp_log.h"

#include "coroutine.h"
#include "file_manager_screens.h"
#include "flash_fs.h"
#include "keyboard_modal.h"
#include "menus_module.h"
#include "modals_module.h"
#include "sd_card.h"

#define INTERNAL_ROOT "/internal"
#define SD_CARD_ROOT  "/sdcard"
#define TAG           "File Manager"

typedef enum {
  FM_CANCELED_OPTION = -1,
  FM_RENAME_OPTION,
  FM_ERASE_OPTION
} file_options_t;

typedef enum {
  FILE_MANAGER_EXIT = -1,
  FILE_MANAGER_ROOT_INTERNAL,
  FILE_MANAGER_ROOT_SDCARD,
} root_selection_t;

static file_manager_context_t* fm_ctx;
static file_manager_show_event_cb_t file_manager_show_event_cb = NULL;
static char* file_options[] = {"Rename", "Delete", NULL};
static void file_manager_input_cb(uint8_t button_name, uint8_t button_event);

static void open_root_options();

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

  fm_ctx->is_root = strcmp(SD_CARD_ROOT, fm_ctx->current_path) == 0 ||
                    strcmp(INTERNAL_ROOT, fm_ctx->current_path) == 0;
  fm_ctx->file_items_arr =
      malloc(fm_ctx->items_count * sizeof(file_item_t*));  // check false

  dir = opendir(fm_ctx->current_path);  // check false

  uint16_t idx = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
      file_item_t* item = malloc(sizeof(file_item_t));  // check false
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
  ctx->file_items_arr = NULL;
  return ctx;
}

static void file_manager_module_exit() {
  clear_items();
  free(fm_ctx);
  menus_module_restart();
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
    start_coroutine(open_root_options, NULL);
  } else {
    get_parent_path(fm_ctx->current_path, fm_ctx->current_path);
    fm_ctx->selected_item = 0;
    refresh_files();
  }
}

void split_filename(const char* filepath, char* filename, char* extension) {
  const char* dot = strrchr(filepath, '.');
  if (dot != NULL) {
    strcpy(extension, dot + 1);
    size_t length = dot - filepath;
    strncpy(filename, filepath, length);
    filename[length] = '\0';
  } else {
    strcpy(filename, filepath);
    extension[0] = '\0';
  }
}

static void file_options_handler(int8_t selection) {
  switch (selection) {
    case FM_RENAME_OPTION:
      char filename[50];
      char extension[10];
      split_filename(fm_ctx->file_items_arr[fm_ctx->selected_item]->name,
                     filename, extension);
      char* new_name = keyboard_modal_write(filename, "     RENAME    ");
      if (new_name != NULL) {
        char* new_path =
            (char*) malloc(strlen(new_name) + strlen(fm_ctx->current_path) +
                           strlen(extension) + 3);
        sprintf(new_path, "%s/%s.%s", fm_ctx->current_path, new_name,
                extension);
        if (rename(fm_ctx->file_items_arr[fm_ctx->selected_item]->path,
                   new_path) == 0) {
          modals_module_show_info("Success", "File was renamed successfully ",
                                  1000, true);
        } else {
          modals_module_show_info("Error", strerror(errno), 2000, true);
        }
        free(new_path);
      }
      menus_module_set_app_state(true, file_manager_input_cb);
      break;
    case FM_ERASE_OPTION:
      if (modals_module_get_user_y_n_selection("  Are You Sure  ") ==
          YES_OPTION) {
        if (remove(fm_ctx->file_items_arr[fm_ctx->selected_item]->path) == 0) {
          modals_module_show_info("Deleted", "File was deleted successfully",
                                  1000, true);
        } else {
          modals_module_show_info("Error", "Something was wrong, try again",
                                  2000, true);
        }
      }
      menus_module_set_app_state(true, file_manager_input_cb);
      break;
    default:
      break;
  }
}

static void open_file_options() {
  int8_t selection = modals_module_get_user_selection(file_options, "< Cancel");
  menus_module_set_app_state(true, file_manager_input_cb);
  file_options_handler(selection);
  update_files();
  fm_ctx->selected_item = MIN(fm_ctx->selected_item, fm_ctx->items_count - 1);
  print_files();
  vTaskDelete(NULL);
}

static void navigation_enter() {
  if (!fm_ctx->items_count) {
    return;
  }
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

static void open_root_directory(char* root) {
  fm_ctx->current_path = root;
  menus_module_set_app_state(true, file_manager_input_cb);
  file_manager_set_show_event_callback(file_manager_screens_event_handler);
  refresh_files();
}

static void open_root_options() {
  char** root_paths = NULL;
  uint8_t root_idx = 0;
  if (flash_fs_mount() == ESP_OK) {
    root_paths = (char**) malloc(sizeof(char*) * (root_idx + 1));
    root_paths[root_idx] = (char*) malloc(sizeof(INTERNAL_ROOT) + 1);
    strcpy(root_paths[root_idx++], "Internal");
  }
  if (sd_card_mount() == ESP_OK) {
    root_paths = (char**) realloc(root_paths, sizeof(char*) * (root_idx + 1));
    root_paths[root_idx] = (char*) malloc(sizeof(SD_CARD_ROOT) + 1);
    strcpy(root_paths[root_idx++], "SD CARD");
  }
  if (!root_idx) {
    modals_module_show_info("ERROR", "No file systems detected", 2000, true);
    file_manager_module_exit();
  }
  root_paths = (char**) realloc(root_paths, sizeof(root_paths) + 1);
  root_paths[root_idx] = NULL;
  int8_t root_selection =
      modals_module_get_user_selection(root_paths, "< Exit");
  root_idx = 0;
  while (root_paths[root_idx] != NULL) {
    free(root_paths[root_idx]);
    root_idx++;
  }
  free(root_paths);
  switch (root_selection) {
    case FILE_MANAGER_EXIT:
      file_manager_module_exit();
      break;
    case FILE_MANAGER_ROOT_INTERNAL:
      open_root_directory(INTERNAL_ROOT);
      break;
    case FILE_MANAGER_ROOT_SDCARD:
      open_root_directory(SD_CARD_ROOT);
      break;
    default:
      break;
  }
  vTaskDelete(NULL);
}

void file_manager_module_init() {
  fm_ctx = file_manager_context_alloc();
  start_coroutine(open_root_options, NULL);
}
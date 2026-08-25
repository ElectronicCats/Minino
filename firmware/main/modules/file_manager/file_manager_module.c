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
  if (fm_ctx->file_items_arr != NULL) {
    for (uint16_t i = 0; i < fm_ctx->items_count; i++) {
      if (fm_ctx->file_items_arr[i] != NULL) {
        if (fm_ctx->file_items_arr[i]->name != NULL) {
          free(fm_ctx->file_items_arr[i]->name);
        }
        if (fm_ctx->file_items_arr[i]->path != NULL) {
          free(fm_ctx->file_items_arr[i]->path);
        }
        free(fm_ctx->file_items_arr[i]);
      }
    }
    free(fm_ctx->file_items_arr);
    fm_ctx->file_items_arr = NULL;
  }
  fm_ctx->items_count = 0;
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

  DIR* dir = opendir(fm_ctx->current_path);
  if (dir == NULL) {
    ESP_LOGE(TAG, "Failed to opendir '%s': %s", fm_ctx->current_path,
             strerror(errno));
    return;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }
      fm_ctx->items_count++;
    }
  }
  closedir(dir);

  fm_ctx->is_root = (strcmp(SD_CARD_ROOT, fm_ctx->current_path) == 0) ||
                    (strcmp(INTERNAL_ROOT, fm_ctx->current_path) == 0);

  if (fm_ctx->items_count == 0) {
    fm_ctx->file_items_arr = NULL;
    return;
  }

  fm_ctx->file_items_arr = malloc(fm_ctx->items_count * sizeof(file_item_t*));
  if (fm_ctx->file_items_arr == NULL) {
    ESP_LOGE(TAG, "Failed to allocate file_items_arr");
    fm_ctx->items_count = 0;
    return;
  }

  dir = opendir(fm_ctx->current_path);
  if (dir == NULL) {
    ESP_LOGE(TAG, "Second opendir failed");
    free(fm_ctx->file_items_arr);
    fm_ctx->file_items_arr = NULL;
    fm_ctx->items_count = 0;
    return;
  }

  uint16_t idx = 0;
  while ((entry = readdir(dir)) != NULL && idx < fm_ctx->items_count) {
    if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }
      file_item_t* item = malloc(sizeof(file_item_t));
      if (item == NULL) {
        continue;
      }
      item->is_dir = (entry->d_type == DT_DIR);
      item->name = strdup(entry->d_name);
      size_t path_len =
          strlen(fm_ctx->current_path) + strlen(entry->d_name) + 2;
      item->path = malloc(path_len);
      if (item->path != NULL) {
        snprintf(item->path, path_len, "%s/%s", fm_ctx->current_path,
                 entry->d_name);
      }
      fm_ctx->file_items_arr[idx++] = item;
    }
  }
  fm_ctx->items_count = idx;
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
  file_manager_context_t* ctx = malloc(sizeof(file_manager_context_t));
  if (ctx != NULL) {
    memset(ctx, 0, sizeof(file_manager_context_t));
    ctx->file_items_arr = NULL;
  }
  return ctx;
}

static void file_manager_module_exit() {
  clear_items();
  if (fm_ctx != NULL) {
    free(fm_ctx);
    fm_ctx = NULL;
  }
  menus_module_restart();
}

static void navigation_up() {
  if (fm_ctx->items_count == 0) {
    return;
  }
  fm_ctx->selected_item = fm_ctx->selected_item == 0
                              ? fm_ctx->items_count - 1
                              : fm_ctx->selected_item - 1;
  print_files();
}

static void navigation_down() {
  if (fm_ctx->items_count == 0) {
    return;
  }
  fm_ctx->selected_item =
      ++fm_ctx->selected_item < fm_ctx->items_count ? fm_ctx->selected_item : 0;
  print_files();
}

static void navigation_back() {
  if (fm_ctx->is_root) {
    start_coroutine(open_root_options, NULL);
  } else {
    char parent[sizeof(fm_ctx->current_path)];
    get_parent_path(fm_ctx->current_path, parent);
    strncpy(fm_ctx->current_path, parent, sizeof(fm_ctx->current_path) - 1);
    fm_ctx->current_path[sizeof(fm_ctx->current_path) - 1] = '\0';
    fm_ctx->selected_item = 0;
    refresh_files();
  }
}

void split_filename(const char* filepath, char* filename, char* extension) {
  if (filepath == NULL || filename == NULL || extension == NULL) {
    return;
  }
  const char* dot = strrchr(filepath, '.');
  if (dot != NULL) {
    strncpy(extension, dot + 1, 9);
    extension[9] = '\0';
    size_t length = dot - filepath;
    if (length > 49) {
      length = 49;
    }
    strncpy(filename, filepath, length);
    filename[length] = '\0';
  } else {
    strncpy(filename, filepath, 49);
    filename[49] = '\0';
    extension[0] = '\0';
  }
}

static void file_options_handler(int8_t selection) {
  if (fm_ctx->selected_item >= fm_ctx->items_count ||
      fm_ctx->file_items_arr == NULL ||
      fm_ctx->file_items_arr[fm_ctx->selected_item] == NULL) {
    return;
  }

  switch (selection) {
    case FM_RENAME_OPTION: {
      char filename[50];
      char extension[10];
      split_filename(fm_ctx->file_items_arr[fm_ctx->selected_item]->name,
                     filename, extension);
      char* new_name = keyboard_modal_write(filename, "     RENAME    ");
      if (new_name != NULL) {
        char* new_path =
            (char*) malloc(strlen(new_name) + strlen(fm_ctx->current_path) +
                           strlen(extension) + 3);
        if (new_path != NULL) {
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
      }
      menus_module_set_app_state(true, file_manager_input_cb);
      break;
    }
    case FM_ERASE_OPTION: {
      if (modals_module_get_user_y_n_selection(" Are You Sure? ") ==
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
    }
    default:
      break;
  }
}

static void open_file_options() {
  int8_t selection = modals_module_get_user_selection(file_options, "< Cancel");
  menus_module_set_app_state(true, file_manager_input_cb);
  file_options_handler(selection);
  update_files();
  if (fm_ctx->items_count > 0) {
    fm_ctx->selected_item = MIN(fm_ctx->selected_item, fm_ctx->items_count - 1);
  } else {
    fm_ctx->selected_item = 0;
  }
  print_files();
  vTaskDelete(NULL);
}

static void navigation_enter() {
  if (!fm_ctx->items_count || fm_ctx->file_items_arr == NULL) {
    return;
  }
  if (fm_ctx->selected_item >= fm_ctx->items_count ||
      fm_ctx->file_items_arr[fm_ctx->selected_item] == NULL) {
    return;
  }
  if (fm_ctx->file_items_arr[fm_ctx->selected_item]->is_dir) {
    char next_path[sizeof(fm_ctx->current_path)];
    snprintf(next_path, sizeof(next_path), "%s",
             fm_ctx->file_items_arr[fm_ctx->selected_item]->path
                 ? fm_ctx->file_items_arr[fm_ctx->selected_item]->path
                 : "");
    strncpy(fm_ctx->current_path, next_path, sizeof(fm_ctx->current_path) - 1);
    fm_ctx->current_path[sizeof(fm_ctx->current_path) - 1] = '\0';
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

static void open_root_directory(const char* root) {
  if (root == NULL) {
    return;
  }
  strncpy(fm_ctx->current_path, root, sizeof(fm_ctx->current_path) - 1);
  fm_ctx->current_path[sizeof(fm_ctx->current_path) - 1] = '\0';
  fm_ctx->selected_item = 0;
  menus_module_set_app_state(true, file_manager_input_cb);
  file_manager_set_show_event_callback(file_manager_screens_event_handler);
  refresh_files();
}

static void open_root_options() {
  char* root_paths[3] = {NULL, NULL, NULL};
  const char* root_targets[2] = {NULL, NULL};
  uint8_t root_idx = 0;

  if (flash_fs_mount() == ESP_OK) {
    root_paths[root_idx] = "Internal";
    root_targets[root_idx] = INTERNAL_ROOT;
    root_idx++;
  }
  if (sd_card_mount() == ESP_OK) {
    root_paths[root_idx] = "SD CARD";
    root_targets[root_idx] = SD_CARD_ROOT;
    root_idx++;
  }

  if (root_idx == 0) {
    modals_module_show_info("ERROR", "No file systems detected", 2000, true);
    file_manager_module_exit();
    vTaskDelete(NULL);
    return;
  }

  root_paths[root_idx] = NULL;
  int8_t root_selection =
      modals_module_get_user_selection(root_paths, "< Exit");

  if (root_selection < 0 || root_selection >= root_idx) {
    file_manager_module_exit();
  } else {
    open_root_directory(root_targets[root_selection]);
  }
  vTaskDelete(NULL);
}

void file_manager_module_init() {
  fm_ctx = file_manager_context_alloc();
  start_coroutine(open_root_options, NULL);
}
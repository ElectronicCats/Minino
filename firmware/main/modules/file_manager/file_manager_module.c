#include "file_manager_module.h"

#include <stdbool.h>
#include <string.h>
#include "dirent.h"

#include "sd_card.h"

#define SD_CARD_ROOT "/sdcard"
#define TAG          "File Manager"

typedef struct {
  bool is_path;
  char* name;
  char* path;
} file_item_t;

typedef struct {
  uint8_t items_count;
  uint8_t selected_item;
  uint8_t last_item;
  bool is_root;
  bool is_main;
  char* current_path;
  char* parent_path;
  file_item_t** file_items_arr;
} file_manager_context_t;

file_manager_context_t* file_manager_ctx;

static void clean_items() {
  file_manager_ctx->items_count = 0;
  for (uint8_t i = 0; i < file_manager_ctx->items_count; i++) {
    free(file_manager_ctx->file_items_arr[i]);
  }
  free(file_manager_ctx->file_items_arr);
}
static void update_files() {
  clean_items();

  DIR* dir;
  struct dirent* entry;
  dir = opendir(file_manager_ctx->current_path);  // check false
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
      file_manager_ctx->items_count++;
    }
  }
  closedir(dir);

  file_manager_ctx->file_items_arr = malloc(
      file_manager_ctx->items_count * sizeof(file_item_t*));  // check false

  dir = opendir(file_manager_ctx->current_path);  // check false

  uint16_t idx = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
      file_item_t* item = malloc(sizeof(file_item_t*));  // check false
      item->is_path = (entry->d_type == DT_DIR);
      item->name = strdup(entry->d_name);  // check false
      size_t path_len =
          strlen(file_manager_ctx->current_path) + strlen(entry->d_name) + 2;
      item->path = malloc(path_len);  // chek false
      snprintf(item->path, path_len, "%s/%s", file_manager_ctx->current_path,
               entry->d_name);
      file_manager_ctx->file_items_arr[idx++] = item;
    }
  }
  closedir(dir);
}

static void print_files() {
  update_files();
  for (uint8_t i = 0; i < file_manager_ctx->items_count; i++) {
    printf("%s\n", file_manager_ctx->file_items_arr[i]->path);
  }
}

static file_manager_context_t* file_manager_context_alloc() {
  file_manager_context_t* ctx =
      malloc(sizeof(file_manager_context_t));  // check false
  memset(ctx, 0, sizeof(file_manager_context_t));
  ctx->current_path = SD_CARD_ROOT;
  ctx->parent_path = NULL;
  ctx->file_items_arr = NULL;
  return ctx;
}

static void file_manager_context_free() {
  clean_items();
  free(file_manager_ctx);
}

void file_manager_module_init() {
  if (sd_card_mount() != ESP_OK)
    return;
  file_manager_ctx = file_manager_context_alloc();
  print_files();
}

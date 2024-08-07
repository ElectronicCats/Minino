#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef enum {
  FILE_MANAGER_UPDATE_LIST_EV,
  FILE_MANAGER_SHOW_FATAL_ERR_EV
} file_manager_events_t;

typedef struct {
  bool is_dir;
  char* name;
  char* path;
} file_item_t;

typedef struct {
  uint8_t items_count;
  uint8_t selected_item;
  bool is_root;
  char* current_path;
  file_item_t** file_items_arr;
} file_manager_context_t;

typedef void (*file_manager_show_event_cb_t)(file_manager_events_t, void*);

void file_manager_module_init();
void file_manager_set_show_event_callback(file_manager_show_event_cb_t cb);
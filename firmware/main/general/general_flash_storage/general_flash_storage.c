#include "general_flash_storage.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "preferences.h"

#define FS_TREE_MAIN_COUNT    "fsmc"
#define FS_TREE_SUBITEM_COUNT "fsmc"
#define FS_TREE_MAIN_PREFIX   "fsm"
#define FS_TREE_SUBITEM_SUFIX "si"
#define MAX_LEN_STRING        1024
#define MAX_NVS_CHARS         15

static const char* TAG = "flash_storage";
static esp_err_t err;

static char* idx_main_item;
static char* main_item;
static char* idx_subitem;
static char* idx_subitem_count;
static char* tree_subitem_str;
static char* tree_subitem;
static char* tree_subitem_val;

void flash_storage_begin() {
  idx_main_item = malloc(MAX_NVS_CHARS);
  if (!idx_main_item)
    goto error;

  main_item = malloc(MAX_NVS_CHARS);
  if (!main_item)
    goto error;

  idx_subitem = malloc(MAX_NVS_CHARS);
  if (!idx_subitem)
    goto error;

  idx_subitem_count = malloc(MAX_NVS_CHARS);
  if (!idx_subitem_count)
    goto error;

  tree_subitem_str = malloc(MAX_NVS_CHARS);
  if (!tree_subitem_str)
    goto error;

  tree_subitem = malloc(MAX_NVS_CHARS);
  if (!tree_subitem)
    goto error;

  tree_subitem_val = malloc(MAX_LEN_STRING);
  if (!tree_subitem_val)
    goto error;

  return;

error:
  ESP_LOGE(TAG, "Out of memory in flash_storage_begin");
  // Liberar lo que se pudo allocar
  if (idx_main_item)
    free(idx_main_item);
  if (main_item)
    free(main_item);
  if (idx_subitem)
    free(idx_subitem);
  if (idx_subitem_count)
    free(idx_subitem_count);
  if (tree_subitem_str)
    free(tree_subitem_str);
  if (tree_subitem)
    free(tree_subitem);
  if (tree_subitem_val)
    free(tree_subitem_val);
}

static bool flash_storage_exist_subitem(char* main_tree, char* subitem) {
  bool return_val = false;

  uint16_t main_count = preferences_get_ushort(FS_TREE_MAIN_COUNT, 0);

  for (int i = 0; i < main_count; i++) {
    sprintf(idx_main_item, "%d%s", i, FS_TREE_MAIN_PREFIX);
    err = preferences_get_string(idx_main_item, main_item, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(main_tree, main_item) == 0) {
      break;
    }
  }

  sprintf(idx_subitem_count, "%sc", main_item);
  uint16_t subitem_count = preferences_get_ushort(idx_subitem_count, 0);
  for (int j = 0; j < subitem_count; j++) {
    sprintf(idx_subitem, "%d%s", j, idx_main_item);
    err = preferences_get_string(idx_subitem, tree_subitem, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(tree_subitem, subitem) == 0) {
      return_val = true;
      break;
    }
  }

  return return_val;
}

static void flash_storage_update_subitem(storage_contex_t* storage_context) {
  esp_err_t err;

  uint16_t main_count = preferences_get_ushort(FS_TREE_MAIN_COUNT, 0);

  for (int i = 0; i < main_count; i++) {
    sprintf(idx_main_item, "%d%s", i, FS_TREE_MAIN_PREFIX);
    err = preferences_get_string(idx_main_item, main_item, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(storage_context->main_storage_name, main_item) == 0) {
      break;
    }
  }

  sprintf(idx_subitem_count, "%sc", main_item);
  uint16_t subitem_count = preferences_get_ushort(idx_subitem_count, 0);
  for (int j = 0; j < subitem_count; j++) {
    sprintf(idx_subitem, "%d%s", j, idx_main_item);
    err = preferences_get_string(idx_subitem, tree_subitem, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(tree_subitem, storage_context->item_storage_name) == 0) {
      sprintf(tree_subitem_str, "%sv", idx_subitem);
      err = preferences_put_string(tree_subitem_str,
                                   storage_context->items_storage_value);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
        continue;
      }
      break;
    }
  }
}

static bool flash_storage_exist_main_item(char* base_name) {
  bool return_val = false;
  esp_err_t err;

  uint16_t main_count = preferences_get_ushort(FS_TREE_MAIN_COUNT, 0);

  for (int i = 0; i < main_count; i++) {
    sprintf(idx_main_item, "%d%s", i, FS_TREE_MAIN_PREFIX);
    err = preferences_get_string(idx_main_item, main_item, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(base_name, main_item) == 0) {
      return_val = true;
      break;
    }
  }
  return return_val;
}

static esp_err_t flash_storage_save_main_item(char* base_name) {
  uint16_t item_count = preferences_get_ushort(FS_TREE_MAIN_COUNT, 0);
  char* idx_item = malloc(MAX_NVS_CHARS);

  sprintf(idx_item, "%d%s", item_count, FS_TREE_MAIN_PREFIX);
  err = preferences_put_string(idx_item, base_name);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "No item saved: %s", esp_err_to_name(err));
    return err;
  }
  item_count++;
  err = preferences_put_ushort(FS_TREE_MAIN_COUNT, item_count);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error saving count: %s", esp_err_to_name(err));
    return err;
  }
  free(idx_item);
  return err;
}

static void flash_storage_save_subitem(storage_contex_t* storage_context) {
  if (flash_storage_exist_subitem(storage_context->main_storage_name,
                                  storage_context->item_storage_name)) {
    ESP_LOGI(TAG, "Updating subitem: %s", storage_context->item_storage_name);
    flash_storage_update_subitem(storage_context);
    return;
  }

  uint16_t main_count = preferences_get_ushort(FS_TREE_MAIN_COUNT, 0);

  for (int i = 0; i < main_count; i++) {
    sprintf(idx_main_item, "%d%s", i, FS_TREE_MAIN_PREFIX);
    err = preferences_get_string(idx_main_item, main_item, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(main_item, storage_context->main_storage_name) == 0) {
      break;
    }
  }

  sprintf(idx_subitem_count, "%sc", main_item);
  uint16_t subitem_count = preferences_get_ushort(idx_subitem_count, 0);
  sprintf(idx_subitem, "%d%s", subitem_count, idx_main_item);

  err = preferences_put_string(idx_subitem, storage_context->item_storage_name);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "No item saved: %s", esp_err_to_name(err));
    return;
  }
  subitem_count++;
  err = preferences_put_ushort(idx_subitem_count, subitem_count);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error saving count: %s", esp_err_to_name(err));
    return;
  }

  sprintf(tree_subitem_val, "%sv", idx_subitem);
  err = preferences_put_string(tree_subitem_val,
                               storage_context->items_storage_value);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "No item saved: %s", esp_err_to_name(err));
    return;
  }
}

void flash_storage_show_list(char* main_tree) {
  esp_err_t err;

  uint16_t main_count = preferences_get_ushort(FS_TREE_MAIN_COUNT, 0);

  for (int i = 0; i < main_count; i++) {
    sprintf(idx_main_item, "%d%s", i, FS_TREE_MAIN_PREFIX);
    err = preferences_get_string(idx_main_item, main_item, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(main_tree, main_item) == 0) {
      break;
    }
  }

  ESP_LOGI(TAG, "Main item: %s", main_tree);

  sprintf(idx_subitem_count, "%sc", main_item);
  uint16_t subitem_count = preferences_get_ushort(idx_subitem_count, 0);
  for (int j = 0; j < subitem_count; j++) {
    sprintf(idx_subitem, "%d%s", j, idx_main_item);
    err = preferences_get_string(idx_subitem, tree_subitem, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    sprintf(tree_subitem_str, "%sv", idx_subitem);
    err = preferences_get_string(tree_subitem_str, tree_subitem_val,
                                 MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    printf("[%d] %s:%s\n", j, tree_subitem, tree_subitem_val);
  }
}

void flash_storage_save_list_items(storage_contex_t* storage_context) {
  if (!flash_storage_exist_main_item(storage_context->main_storage_name)) {
    err = flash_storage_save_main_item(storage_context->main_storage_name);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error saving: %s", esp_err_to_name(err));
      return;
    }
  }

  flash_storage_save_subitem(storage_context);
}

void flash_storage_delete_list_item(char* main_tree, char* subitem) {
  uint16_t main_count = preferences_get_ushort(FS_TREE_MAIN_COUNT, 0);

  for (int i = 0; i < main_count; i++) {
    sprintf(idx_main_item, "%d%s", i, FS_TREE_MAIN_PREFIX);
    err = preferences_get_string(idx_main_item, main_item, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Main item not found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(main_tree, main_item) == 0) {
      break;
    }
  }
  sprintf(idx_subitem_count, "%sc", main_item);
  uint16_t subitem_count = preferences_get_ushort(idx_subitem_count, 0);

  if (subitem_count == 0) {
    ESP_LOGW(TAG, "No subitems to delete.");
    return;
  }

  storage_contex_t* list = malloc(sizeof(storage_contex_t) * subitem_count);
  uint8_t counter_items = 0;

  for (int j = 0; j < subitem_count; j++) {
    sprintf(idx_subitem, "%d%s", j, idx_main_item);
    err = preferences_get_string(idx_subitem, tree_subitem, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Subitem not found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(tree_subitem, subitem) == 0) {
      ESP_LOGI(TAG, "Deleting subitem: %s", tree_subitem);
      continue;
    }
    sprintf(tree_subitem_val, "%sv", idx_subitem);
    err = preferences_get_string(tree_subitem_val, tree_subitem_val,
                                 MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Subitem value not found: %s", esp_err_to_name(err));
      continue;
    }

    list[counter_items].main_storage_name = strdup(main_tree);
    list[counter_items].item_storage_name = strdup(tree_subitem);
    list[counter_items].items_storage_value = strdup(tree_subitem_val);
    counter_items++;
  }

  for (int j = 0; j < subitem_count; j++) {
    sprintf(idx_subitem, "%d%s", j, idx_main_item);
    sprintf(tree_subitem_val, "%sv", idx_subitem);
    preferences_remove(tree_subitem_val);
    preferences_remove(idx_subitem);
  }

  err = preferences_put_ushort(idx_subitem_count, 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error updating subitem count: %s", esp_err_to_name(err));
    return;
  }

  for (int j = 0; j < counter_items; j++) {
    flash_storage_save_subitem(&list[j]);
    free(list[j].item_storage_name);
    free(list[j].items_storage_value);
  }

  free(list);
}

void flash_storage_get_list(char* main_tree,
                            storage_contex_t* list_storage,
                            uint8_t* list_count) {
  esp_err_t err;
  uint16_t main_count = preferences_get_ushort(FS_TREE_MAIN_COUNT, 0);

  for (int i = 0; i < main_count; i++) {
    sprintf(idx_main_item, "%d%s", i, FS_TREE_MAIN_PREFIX);
    err = preferences_get_string(idx_main_item, main_item, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Main item not found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(main_tree, main_item) == 0) {
      break;
    }
  }
  sprintf(idx_subitem_count, "%sc", main_item);
  uint16_t subitem_count = preferences_get_ushort(idx_subitem_count, 0);
  uint8_t counter_items = 0;

  for (int j = 0; j < subitem_count; j++) {
    sprintf(idx_subitem, "%d%s", j, idx_main_item);
    err = preferences_get_string(idx_subitem, tree_subitem, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Subitem not found: %s", esp_err_to_name(err));
      continue;
    }
    sprintf(tree_subitem_val, "%sv", idx_subitem);
    err = preferences_get_string(tree_subitem_val, tree_subitem_val,
                                 MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Subitem value not found: %s", esp_err_to_name(err));
      continue;
    }

    storage_contex_t* item = malloc(sizeof(storage_contex_t));
    if (!item) {
      ESP_LOGE(TAG, "Failed to allocate memory for storage_contex_t");
      break;
    }
    item->main_storage_name = strdup(main_tree);
    item->item_storage_name = strdup(tree_subitem);
    item->items_storage_value = strdup(tree_subitem_val);

    if (!item->main_storage_name || !item->item_storage_name ||
        !item->items_storage_value) {
      ESP_LOGE(TAG, "Failed to allocate memory for strings");
      free(item);
      break;
    }

    list_storage[counter_items++] = *item;
  }

  *list_count = counter_items;
}

storage_contex_t* flash_storage_get_item(char* main_tree, char* subitem) {
  esp_err_t err;

  uint16_t main_count = preferences_get_ushort(FS_TREE_MAIN_COUNT, 0);

  for (int i = 0; i < main_count; i++) {
    sprintf(idx_main_item, "%d%s", i, FS_TREE_MAIN_PREFIX);
    err = preferences_get_string(idx_main_item, main_item, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(main_tree, main_item) == 0) {
      break;
    }
  }

  sprintf(idx_subitem_count, "%sc", main_item);
  uint16_t subitem_count = preferences_get_ushort(idx_subitem_count, 0);
  for (int j = 0; j < subitem_count; j++) {
    sprintf(idx_subitem, "%d%s", j, idx_main_item);
    err = preferences_get_string(idx_subitem, tree_subitem, MAX_LEN_STRING);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
      continue;
    }
    if (strcmp(tree_subitem, subitem) == 0) {
      sprintf(tree_subitem_str, "%sv", idx_subitem);
      err = preferences_get_string(tree_subitem_str, tree_subitem_val,
                                   MAX_LEN_STRING);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "No item found: %s", esp_err_to_name(err));
        continue;
      }
      ESP_LOGI(TAG, "Subitem: %s:%s", tree_subitem, tree_subitem_val);
    }
  }

  storage_contex_t* item_ctx = malloc(sizeof(storage_contex_t));
  item_ctx->main_storage_name = main_tree;
  item_ctx->item_storage_name = tree_subitem;
  item_ctx->items_storage_value = tree_subitem_val;

  return item_ctx;
}
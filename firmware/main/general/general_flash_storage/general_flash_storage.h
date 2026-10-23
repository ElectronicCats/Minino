#pragma once
#include "esp_err.h"

#define GENFLASH_STORAGE_SPAM        "spam"
#define GENFLASH_STORAGE_WIFIAP      "wifiap"
#define GENFLASH_STORAGE_MAX_LEN_STR 1024

typedef struct {
  char* main_storage_name;
  char* item_storage_name;
  char* items_storage_value;
} storage_contex_t;

void flash_storage_begin();

/* @brief Save the value in flash as the form of tree
  @param storage_contex_t* storage_context - The struct with the context to save
*/
void flash_storage_save_list_items(storage_contex_t* storage_context);
/* @brief Show the saved values of the main tree
  @param char main_tree - The name of the tree
*/
void flash_storage_show_list(char* main_tree);
/* @brief Delete the value in flash
  @param char main_tree - The name of the main item
  @param char subitem - The name of the subitem to delete
*/
void flash_storage_delete_list_item(char* main_tree, char* subitem);

/* Capacity the callers allocate for flash_storage_get_list(). The number of
   saved items is only bounded by NVS, so the function used to write past the
   caller's array once more than this many entries existed. */
#define FLASH_STORAGE_MAX_LIST_ITEMS 99

/* Returns the number of items written, and refuses to write more than
   capacity items into list_storage. */
uint8_t flash_storage_get_list(char* main_tree,
                               storage_contex_t* list_storage,
                               uint8_t capacity);

storage_contex_t* flash_storage_get_item(char* main_tree, char* subitem);
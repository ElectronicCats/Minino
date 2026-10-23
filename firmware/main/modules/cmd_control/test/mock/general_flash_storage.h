#pragma once
#include <stdint.h>
#define GENFLASH_STORAGE_SPAM "spam"
#define GENFLASH_STORAGE_MAX_LEN_STR 64

typedef struct {
    char *main_storage_name;
    char *item_storage_name;
    char *items_storage_value;
} storage_contex_t;

/* Must mirror firmware/main/general/general_flash_storage/general_flash_storage.h:
   the API takes the caller's array capacity and returns the number of rows
   written, so an unbounded NVS count cannot overflow the array. */
#define FLASH_STORAGE_MAX_LIST_ITEMS 99
uint8_t flash_storage_get_list(char *main_name, storage_contex_t *list,
                               uint8_t capacity);
void flash_storage_show_list(char *main_name);
void flash_storage_save_list_items(storage_contex_t *storage_context);
void flash_storage_delete_list_item(char *main_tree, char *subitem);

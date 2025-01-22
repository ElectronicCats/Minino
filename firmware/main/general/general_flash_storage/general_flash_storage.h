#pragma once
#include "esp_err.h"

#define GENERAL_FLASH_STORAGE_COUNT_LIMIT 99

typedef enum {
  GFS_SPAM,
  GFS_WIFI,
  GFS_GENERALS,
  GFS_COUNT
} storage_categories_t;

typedef struct {
  char* main_storage_name;
  char* item_storage_name;
  char* items_storage_value;
  storage_categories_t category;
} storage_contex_t;

/* @brief Get the value in flash as the form of prefix_basename
  @param char base_name - The name of the option
  @param char str_value - The array to save the returned value
*/
esp_err_t flash_storage_get_str_item(char* base_name, char* str_value);

/* @brief Save the value in flash as the form of prefix_basename
  @param char base_name - The name of the option
  @param char value - The value
*/
esp_err_t flash_storage_save_str_item(char* base_name, char* value);

/* @brief Delete the value in flash as the form of prefix_basename
  @param char base_name - The name of the option
*/
esp_err_t flash_storage_delete_str_item(char* base_name);

void flash_storage_save_list_items(storage_contex_t* storage_context);
esp_err_t flash_storage_delete_uint32_item(char* base_name);
void flash_storage_show_list(char* main_tree);
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define ESP_ERR_FILE_EXISTS       ESP_ERR_NOT_ALLOWED
#define ESP_ERR_FILE_OPEN_FAILED  ESP_FAIL
#define ESP_ERR_FILE_WRITE_FAILED ESP_FAIL

typedef struct {
  const char* name;
  uint64_t total_space;
  float speed;
  const char* type;
} sd_card_info_t;

/**
 * Initialize the SD card.
 *
 * @return void
 */
void sd_card_begin();

/**
 * Mount the SD card.
 *
 * @return esp_err_t
 *
 * @note return ESP_ERR_NO_MEM if failed to initialize the spi bus.
 * @note return ESP_ERR_NOT_SUPPORTED if the SD card is not formatted with FAT.
 * @note return ESP_ERR_NOT_FOUND if the SD card is not found.
 * @note return ESP_FAIL if the operation failed.
 * @note return ESP_OK if the operation was successful or the card is already
 * mounted.
 */
esp_err_t sd_card_mount();

/**
 * Unmount the SD card.
 *
 * @return esp_err_t
 *
 * @note return ESP_ERR_NOT_FOUND if the SD card is not mounted.
 * @note return ESP_FAIL if the operation failed.
 * @note return ESP_OK if the operation was successful.
 */
esp_err_t sd_card_unmount();

/**
 * Mount the SD card.
 *
 * @return esp_err_t
 * @note return ESP_ERR_NO_MEM if failed to initialize the spi bus.
 * @note return ESP_ERR_NOT_SUPPORTED if the SD card is not formatted with FAT.
 * @note return ESP_ERR_INVALID_ARG if the arguments are invalid.
 * @note return ESP_FAIL if the operation failed.
 * @note return ESP_OK if the operation was successful or the card is already
 * mounted.
 */
esp_err_t sd_card_check_format();

/**
 * Format the SD card.
 *
 * @return esp_err_t
 */
esp_err_t sd_card_format();

/**
 * Check if the SD card is mounted.
 *
 * return bool
 */
bool sd_card_is_mounted();

/**
 * Check if the SD card is not mounted.
 *
 * return bool
 */
bool sd_card_is_not_mounted();

/**
 * @brief Create a directory in the SD card.
 *
 * @param dir_name The name of the directory to create.
 *
 * @return esp_err_t
 *
 * @note return ESP_ERR_NOT_FOUND if the SD card is not mounted.
 * @note return ESP_OK if the operation was successful or the directory already
 * exists.
 * @note return ESP_FAIL if the operation failed.
 */
esp_err_t sd_card_create_dir(const char* dir_name);

/**
 * Create a file in the SD card.
 *
 * @param path The path of the file to create.
 *
 * @return esp_err_t
 *
 * @note return ESP_ERR_NOT_FOUND if the SD card is not mounted.
 * @note return ESP_ERR_FILE_EXISTS if the file already exists.
 * @note return ESP_FAIL if the operation failed.
 * @note return ESP_OK if the operation was successful.
 */
esp_err_t sd_card_create_file(const char* path);

/**
 * Read a file from the SD card.
 *
 * @param path The path of the file to read.
 *
 * @return esp_err_t
 *
 * @note return ESP_ERR_NOT_FOUND if the file does not exist.
 */
esp_err_t sd_card_read_file(const char* path);

/**
 * Write data to a file in the SD card.
 * Create the file if it does not exist.
 *
 * @param path The path of the file to write to.
 * @param data The data to write to the file.
 *
 * @return esp_err_t
 *
 * @note return ESP_ERR_NOT_FOUND if the file does not exist.
 */
esp_err_t sd_card_write_file(const char* path, char* data);

/**
 * Get the information of the SD card.
 *
 * @return sd_card_info_t
 */
sd_card_info_t sd_card_get_info();

esp_err_t sd_card_append_to_file(const char* path, char* data);
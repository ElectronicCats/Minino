#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define ESP_ERR_ALREADY_MOUNTED   ESP_ERR_NOT_ALLOWED
#define ESP_ERR_NOT_MOUNTED       ESP_ERR_NOT_FOUND
#define ESP_ERR_FILE_EXISTS       ESP_ERR_NOT_ALLOWED
#define ESP_ERR_FILE_OPEN_FAILED  ESP_FAIL
#define ESP_ERR_FILE_WRITE_FAILED ESP_FAIL

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
 * @note return ESP_ERR_NOT_FOUND if the SD card is not found.
 * @note return ESP_ERR_ALREADY_MOUNTED if the SD card is already mounted.
 * @note return ESP_ERR_NO_MEM if failed to initialize the spi bus.
 * @note return ESP_ERR_NOT_SUPPORTED if the SD card is not formatted with FAT.
 * @note return ESP_ERR_INVALID_ARG if the arguments are invalid.
 * @note return ESP_FAIL if the operation failed.
 * @note return ESP_OK if the operation was successful.
 */
esp_err_t sd_card_mount();

/**
 * Unmount the SD card.
 *
 * @return esp_err_t
 *
 * @note return ESP_ERR_NOT_MOUNTED if the SD card is not mounted.
 * @note return ESP_FAIL if the operation failed.
 * @note return ESP_OK if the operation was successful.
 */
esp_err_t sd_card_unmount();

/**
 * Check if the SD card is mounted.
 *
 * return bool
 */
bool sd_card_is_mounted();

/**
 * Create a file in the SD card.
 *
 * @param path The path of the file to create.
 *
 * @return esp_err_t
 *
 * @note return ESP_ERR_NOT_MOUNTED if the SD card is not mounted.
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

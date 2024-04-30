#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"

/**
 * @brief Begin preferences
 *
 * @return esp_err_t
 */
esp_err_t preferences_begin();

/**
 * @brief End preferences
 */
void preferences_end();

/**
 * @brief Clear preferences
 *
 * @return esp_err_t
 */
esp_err_t preferences_clear();

/**
 * @brief Remove a key from preferences
 *
 * @param key
 *
 * @return esp_err_t
 */
esp_err_t preferences_remove(const char* key);

/**
 * @brief Put a char value (int8_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_char(const char* key, int8_t value);

/**
 * @brief Put an unsigned char value (uint8_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_uchar(const char* key, uint8_t value);

/**
 * @brief Put a short value (int16_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_short(const char* key, int16_t value);

/**
 * @brief Put an unsigned short value (uint16_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_ushort(const char* key, uint16_t value);

/**
 * @brief Put an integer value (int32_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_int(const char* key, int32_t value);

/**
 * @brief Put an unsigned integer value (uint32_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_uint(const char* key, uint32_t value);

/**
 * @brief Put a long value (int32_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_long(const char* key, int32_t value);

/**
 * @brief Put an unsigned long value (int32_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_ulong(const char* key, uint32_t value);

/**
 * @brief Put a 64-bit integer value (int64_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_long64(const char* key, int64_t value);

/**
 * @brief Put an unsigned 64-bit integer value (uint64_t) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_ulong64(const char* key, uint64_t value);

/**
 * @brief Put a float value in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_float(const char* key, float value);

/**
 * @brief Put a double value in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_double(const char* key, double value);

/**
 * @brief Put a boolean value in preferences
 *
 * @param key
 * @param default_value
 *
 * @return bool
 */
esp_err_t preferences_put_bool(const char* key, bool value);

/**
 * @brief Put a string value (const char*) in preferences
 *
 * @param key
 * @param value
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_string(const char* key, const char* value);

/**
 * @brief Put a byte array (const void*) in preferences
 *
 * @param key
 * @param value
 * @param length
 *
 * @return esp_err_t
 */
esp_err_t preferences_put_bytes(const char* key,
                                const void* value,
                                size_t length);

/**
 * @brief Get a char value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return char
 */
int8_t preferences_get_char(const char* key, int8_t default_value);

/**
 * @brief Get an unsigned char value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return uint8_t
 */
uint8_t preferences_get_uchar(const char* key, uint8_t default_value);

/**
 * @brief Get a short value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return int16_t
 */
int16_t preferences_get_short(const char* key, int16_t default_value);

/**
 * @brief Get an unsigned short value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return uint16_t
 */
uint16_t preferences_get_ushort(const char* key, uint16_t default_value);

/**
 * @brief Get an integer value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return int32_t
 */
int32_t preferences_get_int(const char* key, int32_t default_value);

/**
 * @brief Get an unsigned integer value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return uint32_t
 */
uint32_t preferences_get_uint(const char* key, uint32_t default_value);

/**
 * @brief Get a long value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return int32_t
 */
int32_t preferences_get_long(const char* key, int32_t default_value);

/**
 * @brief Get an unsigned long value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return uint32_t
 */
uint32_t preferences_get_ulong(const char* key, uint32_t default_value);

/**
 * @brief Get a 64-bit integer value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return int64_t
 */
int64_t preferences_get_long64(const char* key, int64_t default_value);

/**
 * @brief Get an unsigned 64-bit integer value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return uint64_t
 */
uint64_t preferences_get_ulong64(const char* key, uint64_t default_value);

/**
 * @brief Get a float value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return float
 */
float preferences_get_float(const char* key, float default_value);

/**
 * @brief Get a double value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return double
 */
double preferences_get_double(const char* key, double default_value);

/**
 * @brief Get a boolean value from preferences
 *
 * @param key
 * @param default_value
 *
 * @return bool
 */
bool preferences_get_bool(const char* key, bool default_value);

/**
 * @brief Get a string value from preferences
 *
 * @param key
 * @param value
 * @param max_length
 *
 * @return esp_err_t
 */
esp_err_t preferences_get_string(const char* key,
                                 char* value,
                                 size_t max_length);

/**
 * @brief Get a length of a byte array from preferences
 *
 * @param key
 *
 * @return size_t
 */
size_t preferences_get_bytes_length(const char* key);

/**
 * @brief Get a byte array from preferences
 *
 * @param key
 * @param buffer
 * @param length
 *
 * @return esp_err_t
 */
esp_err_t preferences_get_bytes(const char* key, void* buffer, size_t length);

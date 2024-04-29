#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

size_t preferences_begin();
size_t preferences_end();
size_t preferences_clear();
size_t preferences_remove();
size_t preferences_put_int(const char* key, int32_t value);
int32_t preferences_get_int(const char* key, int32_t default_value);

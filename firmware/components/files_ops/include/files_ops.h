#pragma once

#include <stdbool.h>
#include <stdio.h>

void files_ops_incremental_name(const char* dir_path,
                                const char* base_name,
                                const char* extension,
                                char* path_ptr);
size_t files_ops_get_file_size_2(const char* filepath);
char* files_ops_format_size(size_t size);
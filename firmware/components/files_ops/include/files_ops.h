#pragma once

#include <stdbool.h>
#include <stdio.h>

void files_ops_incremental_name(const char* dir_path,
                                const char* base_name,
                                const char* extension,
                                char* path_ptr);
bool files_ops_incremental_name_buf(const char* dir_path,
                                    const char* base_name,
                                    const char* extension,
                                    char* out_buf,
                                    size_t out_len);

size_t files_ops_get_file_size(FILE* file);
size_t files_ops_get_file_size_2(const char* filepath);
char* files_ops_format_size(size_t size);
bool files_ops_format_size_buf(size_t size, char* out_buf, size_t buf_len);

bool files_ops_exists(const char* dir_path,
                      const char* name,
                      const char* extension);
bool files_ops_has_path_traversal(const char* path);
bool files_ops_is_valid_filename(const char* filename);
bool files_ops_join_path(const char* dir_path,
                         const char* name,
                         const char* extension,
                         char* out_buf,
                         size_t out_len);
bool files_ops_sanitize_filename(const char* input,
                                 char* output,
                                 size_t out_len);
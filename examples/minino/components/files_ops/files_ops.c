#include "files_ops.h"
#include <stdbool.h>
#include <stdio.h>

size_t files_ops_get_file_size(FILE* file) {
  if (file == NULL) {
    return 0;
  }
  long current_ptr = ftell(file);
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, current_ptr);
  return file_size;
}

bool files_ops_exists(const char* dir_path,
                      const char* name,
                      const char* extension) {
  char* full_path = (char*) malloc(256);
  sprintf(full_path, "%s/%s%s", dir_path, name, extension);

  FILE* file = fopen(full_path, "r");
  if (file) {
    fclose(file);
    free(full_path);
    return true;
  } else {
    free(full_path);
    return false;
  }
}

void files_ops_incremental_name(const char* dir_path,
                                const char* base_name,
                                const char* extension,
                                char* path_ptr) {
  int idx = 0;
  bool idx_found = false;
  char* indexed_name = (char*) malloc(32);
  while (!idx_found) {
    sprintf(indexed_name, "%s%02d", base_name, idx);
    if (files_ops_exists(dir_path, indexed_name, extension)) {
      idx++;
    } else {
      idx_found = true;
    }
  }
  sprintf(path_ptr, "%s/%s%s", dir_path, indexed_name, extension);
  free(indexed_name);
}
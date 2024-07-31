#include "files_ops.h"

#include <sys/stat.h>

#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

char* files_ops_format_size(size_t size) {
  char* size_str = (char*) malloc(50);
  if (size_str == NULL) {
    perror("malloc");
    return NULL;
  }

  if (size >= GB) {
    snprintf(size_str, 50, "%.2fGB", (double) size / GB);
  } else if (size >= MB) {
    snprintf(size_str, 50, "%.2fMB", (double) size / MB);
  } else if (size >= KB) {
    snprintf(size_str, 50, "%.2fKB", (double) size / KB);
  } else {
    snprintf(size_str, 50, "%zuB", size);
  }

  return size_str;
}

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

size_t files_ops_get_file_size_2(const char* filepath) {
  struct stat file_stat;
  if (stat(filepath, &file_stat) == -1) {
    perror("stat");
    return 0;
  }
  return (size_t) file_stat.st_size;
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
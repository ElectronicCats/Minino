#include "files_ops.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define KB (1024ULL)
#define MB (1024ULL * KB)
#define GB (1024ULL * MB)

bool files_ops_has_path_traversal(const char* path) {
  if (path == NULL || path[0] == '\0') {
    return true;
  }

  // Check for URL-encoded traversal: %2e = '.', %2f = '/', %5c = '\'
  const char* p = path;
  while (*p != '\0') {
    if (p[0] == '%' && p[1] != '\0' && p[2] != '\0') {
      char c1 = (char) tolower((unsigned char) p[1]);
      char c2 = (char) tolower((unsigned char) p[2]);
      if (c1 == '2' && c2 == 'e') {
        return true;  // %2e
      }
      if (c1 == '2' && c2 == 'f') {
        return true;  // %2f
      }
      if (c1 == '5' && c2 == 'c') {
        return true;  // %5c
      }
    }
    p++;
  }

  // Check path segments between delimiters ('/' and '\\')
  const char* start = path;
  while (*start != '\0') {
    while (*start == '/' || *start == '\\') {
      start++;
    }
    if (*start == '\0') {
      break;
    }

    const char* end = start;
    while (*end != '\0' && *end != '/' && *end != '\\') {
      end++;
    }

    size_t seg_len = (size_t) (end - start);
    if (seg_len == 2 && start[0] == '.' && start[1] == '.') {
      return true;
    }

    start = end;
  }

  return false;
}

bool files_ops_is_valid_filename(const char* filename) {
  if (filename == NULL || filename[0] == '\0') {
    return false;
  }

  size_t len = strlen(filename);
  if (len > 255) {
    return false;
  }

  if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char) filename[i];
    if (c < 32 || c == 127) {
      return false;  // Control characters
    }
    if (c == '/' || c == '\\') {
      return false;  // Path delimiters not allowed in a pure filename
    }
  }

  return !files_ops_has_path_traversal(filename);
}

bool files_ops_sanitize_filename(const char* input,
                                 char* output,
                                 size_t out_len) {
  if (input == NULL || output == NULL || out_len == 0) {
    if (output && out_len > 0) {
      output[0] = '\0';
    }
    return false;
  }

  // Skip leading dots to avoid hidden files and traversal
  while (*input == '.') {
    input++;
  }

  if (*input == '\0') {
    if (out_len >= 8) {
      snprintf(output, out_len, "default");
      return true;
    }
    output[0] = '\0';
    return false;
  }

  size_t written = 0;
  while (*input != '\0' && written + 1 < out_len) {
    unsigned char c = (unsigned char) *input;
    if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':' || c == '*' ||
        c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      output[written++] = '_';
    } else {
      output[written++] = (char) c;
    }
    input++;
  }
  output[written] = '\0';
  return (written > 0);
}

bool files_ops_join_path(const char* dir_path,
                         const char* name,
                         const char* extension,
                         char* out_buf,
                         size_t out_len) {
  if (dir_path == NULL || name == NULL || out_buf == NULL || out_len == 0) {
    if (out_buf && out_len > 0) {
      out_buf[0] = '\0';
    }
    return false;
  }

  if (files_ops_has_path_traversal(dir_path) ||
      files_ops_has_path_traversal(name)) {
    out_buf[0] = '\0';
    return false;
  }

  if (!files_ops_is_valid_filename(name)) {
    out_buf[0] = '\0';
    return false;
  }

  const char* ext = (extension != NULL) ? extension : "";
  if (ext[0] != '\0') {
    if (strchr(ext, '/') != NULL || strchr(ext, '\\') != NULL ||
        strstr(ext, "..") != NULL || files_ops_has_path_traversal(ext)) {
      out_buf[0] = '\0';
      return false;
    }
  }

  // Trim trailing slash from dir_path unless dir_path is "/"
  size_t dir_len = strlen(dir_path);
  while (dir_len > 1 &&
         (dir_path[dir_len - 1] == '/' || dir_path[dir_len - 1] == '\\')) {
    dir_len--;
  }

  bool need_separator = (dir_len > 0 && dir_path[dir_len - 1] != '/' &&
                         dir_path[dir_len - 1] != '\\');
  size_t name_len = strlen(name);
  size_t ext_len = strlen(ext);

  size_t total_len = dir_len + (need_separator ? 1 : 0) + name_len + ext_len;
  if (total_len + 1 > out_len) {
    out_buf[0] = '\0';
    return false;
  }

  int ret = 0;
  if (need_separator) {
    ret = snprintf(out_buf, out_len, "%.*s/%s%s", (int) dir_len, dir_path, name,
                   ext);
  } else {
    ret = snprintf(out_buf, out_len, "%.*s%s%s", (int) dir_len, dir_path, name,
                   ext);
  }

  if (ret < 0 || (size_t) ret >= out_len) {
    out_buf[0] = '\0';
    return false;
  }

  out_buf[out_len - 1] = '\0';
  return true;
}

bool files_ops_format_size_buf(size_t size, char* out_buf, size_t buf_len) {
  if (out_buf == NULL || buf_len == 0) {
    return false;
  }

  int ret = 0;
  if (size >= GB) {
    ret = snprintf(out_buf, buf_len, "%.2fGB", (double) size / (double) GB);
  } else if (size >= MB) {
    ret = snprintf(out_buf, buf_len, "%.2fMB", (double) size / (double) MB);
  } else if (size >= KB) {
    ret = snprintf(out_buf, buf_len, "%.2fKB", (double) size / (double) KB);
  } else {
    ret = snprintf(out_buf, buf_len, "%zuB", size);
  }

  if (ret < 0 || (size_t) ret >= buf_len) {
    out_buf[0] = '\0';
    return false;
  }
  return true;
}

char* files_ops_format_size(size_t size) {
  char* size_str = (char*) malloc(32);
  if (size_str == NULL) {
    return NULL;
  }

  if (!files_ops_format_size_buf(size, size_str, 32)) {
    free(size_str);
    return NULL;
  }

  return size_str;
}

size_t files_ops_get_file_size(FILE* file) {
  if (file == NULL) {
    return 0;
  }

  long current_ptr = ftell(file);
  if (current_ptr < 0) {
    return 0;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    return 0;
  }

  long end_pos = ftell(file);
  fseek(file, current_ptr, SEEK_SET);

  if (end_pos < 0) {
    return 0;
  }

  return (size_t) end_pos;
}

size_t files_ops_get_file_size_2(const char* filepath) {
  if (filepath == NULL || filepath[0] == '\0' ||
      files_ops_has_path_traversal(filepath)) {
    return 0;
  }

  struct stat file_stat;
  if (stat(filepath, &file_stat) != 0) {
    return 0;
  }

  if (file_stat.st_size < 0) {
    return 0;
  }

  return (size_t) file_stat.st_size;
}

bool files_ops_exists(const char* dir_path,
                      const char* name,
                      const char* extension) {
  char full_path[512];
  if (!files_ops_join_path(dir_path, name, extension, full_path,
                           sizeof(full_path))) {
    return false;
  }

  struct stat st;
  return (stat(full_path, &st) == 0);
}

bool files_ops_incremental_name_buf(const char* dir_path,
                                    const char* base_name,
                                    const char* extension,
                                    char* out_buf,
                                    size_t out_len) {
  if (dir_path == NULL || base_name == NULL || out_buf == NULL ||
      out_len == 0) {
    if (out_buf && out_len > 0) {
      out_buf[0] = '\0';
    }
    return false;
  }

  if (files_ops_has_path_traversal(dir_path) ||
      files_ops_has_path_traversal(base_name)) {
    out_buf[0] = '\0';
    return false;
  }

  const char* ext = (extension != NULL) ? extension : "";
  if (ext[0] != '\0' &&
      (strchr(ext, '/') || strchr(ext, '\\') || strstr(ext, "..") ||
       files_ops_has_path_traversal(ext))) {
    out_buf[0] = '\0';
    return false;
  }

  int idx = 0;
  bool idx_found = false;
  char indexed_name[128];

  while (!idx_found && idx < 10000) {
    int ret =
        snprintf(indexed_name, sizeof(indexed_name), "%s%02d", base_name, idx);
    if (ret < 0 || (size_t) ret >= sizeof(indexed_name)) {
      out_buf[0] = '\0';
      return false;
    }

    if (files_ops_exists(dir_path, indexed_name, ext)) {
      idx++;
    } else {
      idx_found = true;
    }
  }

  if (!idx_found) {
    out_buf[0] = '\0';
    return false;
  }

  return files_ops_join_path(dir_path, indexed_name, ext, out_buf, out_len);
}

void files_ops_incremental_name(const char* dir_path,
                                const char* base_name,
                                const char* extension,
                                char* path_ptr) {
  if (path_ptr == NULL) {
    return;
  }
  // Legacy function assumed buffer of at least 256 bytes
  if (!files_ops_incremental_name_buf(dir_path, base_name, extension, path_ptr,
                                      256)) {
    path_ptr[0] = '\0';
  }
}
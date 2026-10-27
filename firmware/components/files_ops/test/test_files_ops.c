#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "files_ops.h"
#include "unity/unity.h"

static char test_temp_dir[128];

void setUp(void) {
  // Create a dedicated temporary directory for file-based tests
  snprintf(test_temp_dir, sizeof(test_temp_dir), "/tmp/files_ops_test_%d",
           (int) getpid());
  mkdir(test_temp_dir, 0700);
}

void tearDown(void) {
  // Remove files and temp dir
  char rm_cmd[512];
  snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", test_temp_dir);
  (void) system(rm_cmd);
}

// ============================================================================
// 1. Path Traversal Tests
// ============================================================================

void test_path_traversal_null_and_empty(void) {
  TEST_ASSERT_TRUE(files_ops_has_path_traversal(NULL));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal(""));
}

void test_path_traversal_direct_dotdot(void) {
  TEST_ASSERT_TRUE(files_ops_has_path_traversal(".."));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("../"));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("../passwd"));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("/.."));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("/../secret"));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("a/b/../../c"));
  TEST_ASSERT_TRUE(
      files_ops_has_path_traversal("/sdcard/captures/../../internal/key"));
}

void test_path_traversal_backslashes(void) {
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("..\\windows"));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("foo\\..\\bar"));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("/path/..\\evil"));
}

void test_path_traversal_url_encoded(void) {
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("%2e%2e/etc/passwd"));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("/dir/%2e%2e"));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("%2E%2E/test"));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("dir/%2f/../bad"));
  TEST_ASSERT_TRUE(files_ops_has_path_traversal("dir/%5c.."));
}

void test_path_traversal_legitimate_paths(void) {
  TEST_ASSERT_FALSE(files_ops_has_path_traversal("/spiffs/file.txt"));
  TEST_ASSERT_FALSE(files_ops_has_path_traversal("/sdcard/wifi.pcap"));
  TEST_ASSERT_FALSE(files_ops_has_path_traversal("capture_01.pcap"));
  TEST_ASSERT_FALSE(files_ops_has_path_traversal("archive.tar.gz"));
  TEST_ASSERT_FALSE(files_ops_has_path_traversal(".hidden_file"));
  TEST_ASSERT_FALSE(files_ops_has_path_traversal("my..dots..in..name"));
}

// ============================================================================
// 2. Filename Validation Tests
// ============================================================================

void test_filename_validation_valid(void) {
  TEST_ASSERT_TRUE(files_ops_is_valid_filename("capture.pcap"));
  TEST_ASSERT_TRUE(files_ops_is_valid_filename("test_01.txt"));
  TEST_ASSERT_TRUE(files_ops_is_valid_filename("valid-filename.bin"));
  TEST_ASSERT_TRUE(files_ops_is_valid_filename("archive.tar.gz"));
}

void test_filename_validation_invalid(void) {
  TEST_ASSERT_FALSE(files_ops_is_valid_filename(NULL));
  TEST_ASSERT_FALSE(files_ops_is_valid_filename(""));
  TEST_ASSERT_FALSE(files_ops_is_valid_filename("."));
  TEST_ASSERT_FALSE(files_ops_is_valid_filename(".."));
  TEST_ASSERT_FALSE(files_ops_is_valid_filename("dir/file.txt"));
  TEST_ASSERT_FALSE(files_ops_is_valid_filename("dir\\file.txt"));
  TEST_ASSERT_FALSE(files_ops_is_valid_filename("../escape.txt"));
  TEST_ASSERT_FALSE(files_ops_is_valid_filename("name\nwith\nnewline"));
  TEST_ASSERT_FALSE(files_ops_is_valid_filename("name\x1bwith\x07control"));

  // Exceeds 255 chars
  char huge_name[300];
  memset(huge_name, 'a', 260);
  huge_name[260] = '\0';
  TEST_ASSERT_FALSE(files_ops_is_valid_filename(huge_name));
}

// ============================================================================
// 3. Filename Sanitization Tests
// ============================================================================

void test_filename_sanitization(void) {
  char out[64];

  TEST_ASSERT_TRUE(
      files_ops_sanitize_filename("my:file*name?.txt", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("my_file_name_.txt", out);

  TEST_ASSERT_TRUE(
      files_ops_sanitize_filename("dir/sub\\path", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("dir_sub_path", out);

  TEST_ASSERT_TRUE(
      files_ops_sanitize_filename("..leading_dots", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("leading_dots", out);

  TEST_ASSERT_TRUE(files_ops_sanitize_filename("....", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("default", out);

  TEST_ASSERT_FALSE(files_ops_sanitize_filename(NULL, out, sizeof(out)));
  TEST_ASSERT_FALSE(files_ops_sanitize_filename("valid", NULL, 0));

  // Small buffer truncates cleanly
  char small_buf[5];
  TEST_ASSERT_TRUE(
      files_ops_sanitize_filename("abcdefgh", small_buf, sizeof(small_buf)));
  TEST_ASSERT_EQUAL_STRING("abcd", small_buf);
}

// ============================================================================
// 4. Safe Path Joining Tests
// ============================================================================

void test_join_path_normal(void) {
  char out[128];
  TEST_ASSERT_TRUE(
      files_ops_join_path("/sdcard", "capture", ".pcap", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("/sdcard/capture.pcap", out);
}

void test_join_path_trailing_slash(void) {
  char out[128];
  // Avoids double slashes: "/sdcard/" + "file" -> "/sdcard/file"
  TEST_ASSERT_TRUE(
      files_ops_join_path("/sdcard/", "file", ".txt", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("/sdcard/file.txt", out);

  // Root directory
  TEST_ASSERT_TRUE(files_ops_join_path("/", "boot", ".bin", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("/boot.bin", out);
}

void test_join_path_no_extension(void) {
  char out[128];
  TEST_ASSERT_TRUE(
      files_ops_join_path("/internal", "test_file", NULL, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("/internal/test_file", out);

  TEST_ASSERT_TRUE(
      files_ops_join_path("/internal", "test_file", "", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("/internal/test_file", out);
}

void test_join_path_rejection(void) {
  char out[128];

  // Path traversal in dir
  TEST_ASSERT_FALSE(
      files_ops_join_path("/sdcard/../etc", "shadow", "", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);

  // Path traversal in name
  TEST_ASSERT_FALSE(
      files_ops_join_path("/sdcard", "../evil", ".pcap", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);

  // Delimiter in name
  TEST_ASSERT_FALSE(
      files_ops_join_path("/sdcard", "sub/file", ".txt", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);

  // Delimiter or traversal in extension
  TEST_ASSERT_FALSE(
      files_ops_join_path("/sdcard", "file", "/bad_ext", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);
  TEST_ASSERT_FALSE(
      files_ops_join_path("/sdcard", "file", "..evil", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);

  // Null checks
  TEST_ASSERT_FALSE(
      files_ops_join_path(NULL, "file", ".txt", out, sizeof(out)));
  TEST_ASSERT_FALSE(
      files_ops_join_path("/sdcard", NULL, ".txt", out, sizeof(out)));
  TEST_ASSERT_FALSE(files_ops_join_path("/sdcard", "file", ".txt", NULL, 0));
}

void test_join_path_buffer_bounds(void) {
  char out[10];
  // "/sdcard/file.txt" requires 17 bytes; 10 is too small
  TEST_ASSERT_FALSE(
      files_ops_join_path("/sdcard", "file", ".txt", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);
}

// ============================================================================
// 5. Size Formatting Tests
// ============================================================================

void test_format_size_buffer(void) {
  char buf[32];

  TEST_ASSERT_TRUE(files_ops_format_size_buf(0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("0B", buf);

  TEST_ASSERT_TRUE(files_ops_format_size_buf(512, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("512B", buf);

  TEST_ASSERT_TRUE(files_ops_format_size_buf(1023, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1023B", buf);

  TEST_ASSERT_TRUE(files_ops_format_size_buf(1024, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1.00KB", buf);

  TEST_ASSERT_TRUE(files_ops_format_size_buf(1536, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1.50KB", buf);

  TEST_ASSERT_TRUE(files_ops_format_size_buf(1048576, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1.00MB", buf);

  TEST_ASSERT_TRUE(files_ops_format_size_buf(5242880, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("5.00MB", buf);

  TEST_ASSERT_TRUE(files_ops_format_size_buf(1073741824ULL, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1.00GB", buf);

  TEST_ASSERT_TRUE(files_ops_format_size_buf(2684354560ULL, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("2.50GB", buf);
}

void test_format_size_allocated(void) {
  char* str = files_ops_format_size(2048);
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("2.00KB", str);
  free(str);

  str = files_ops_format_size(100);
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("100B", str);
  free(str);
}

void test_format_size_buf_overflow_prevention(void) {
  char small_buf[3];
  // "1.00KB" needs at least 7 bytes; 3 is insufficient
  TEST_ASSERT_FALSE(
      files_ops_format_size_buf(1024, small_buf, sizeof(small_buf)));
  TEST_ASSERT_EQUAL_STRING("", small_buf);
}

// ============================================================================
// 6. File Sizing and Descriptor Tests
// ============================================================================

void test_get_file_size_null(void) {
  TEST_ASSERT_EQUAL_UINT32(0, files_ops_get_file_size(NULL));
  TEST_ASSERT_EQUAL_UINT32(0, files_ops_get_file_size_2(NULL));
}

void test_get_file_size_nonexistent(void) {
  TEST_ASSERT_EQUAL_UINT32(
      0, files_ops_get_file_size_2("/path/does/not/exist_12345.bin"));
}

void test_get_file_size_traversal_rejection(void) {
  TEST_ASSERT_EQUAL_UINT32(0, files_ops_get_file_size_2("../../etc/passwd"));
}

void test_get_file_size_with_real_file(void) {
  char filepath[512];
  snprintf(filepath, sizeof(filepath), "%s/test_128b.dat", test_temp_dir);

  FILE* f = fopen(filepath, "wb");
  TEST_ASSERT_NOT_NULL(f);
  uint8_t dummy_data[128];
  memset(dummy_data, 0xAA, sizeof(dummy_data));
  size_t written = fwrite(dummy_data, 1, sizeof(dummy_data), f);
  TEST_ASSERT_EQUAL_UINT32(128, written);
  fclose(f);

  // Test filepath version (files_ops_get_file_size_2)
  size_t size2 = files_ops_get_file_size_2(filepath);
  TEST_ASSERT_EQUAL_UINT32(128, size2);

  // Test FILE* version (files_ops_get_file_size)
  f = fopen(filepath, "rb");
  TEST_ASSERT_NOT_NULL(f);

  // Advance pointer to test seek restoration
  fseek(f, 32, SEEK_SET);
  long pos_before = ftell(f);
  TEST_ASSERT_EQUAL(32, pos_before);

  size_t size1 = files_ops_get_file_size(f);
  TEST_ASSERT_EQUAL_UINT32(128, size1);

  // Verify seek position is restored
  long pos_after = ftell(f);
  TEST_ASSERT_EQUAL(32, pos_after);

  fclose(f);
}

// ============================================================================
// 7. Incremental Name and Existence Tests
// ============================================================================

void test_exists_and_incremental_name(void) {
  char out_path[256];

  // In empty directory, index should be 00
  TEST_ASSERT_TRUE(files_ops_incremental_name_buf(
      test_temp_dir, "analizer", ".pcap", out_path, sizeof(out_path)));

  char expected_path[512];
  snprintf(expected_path, sizeof(expected_path), "%s/analizer00.pcap",
           test_temp_dir);
  TEST_ASSERT_EQUAL_STRING(expected_path, out_path);
  TEST_ASSERT_FALSE(files_ops_exists(test_temp_dir, "analizer00", ".pcap"));

  // Create analizer00.pcap
  FILE* f = fopen(out_path, "w");
  TEST_ASSERT_NOT_NULL(f);
  fputs("data", f);
  fclose(f);

  TEST_ASSERT_TRUE(files_ops_exists(test_temp_dir, "analizer00", ".pcap"));

  // Next incremental call should now yield analizer01.pcap
  TEST_ASSERT_TRUE(files_ops_incremental_name_buf(
      test_temp_dir, "analizer", ".pcap", out_path, sizeof(out_path)));
  snprintf(expected_path, sizeof(expected_path), "%s/analizer01.pcap",
           test_temp_dir);
  TEST_ASSERT_EQUAL_STRING(expected_path, out_path);

  // Legacy wrapper files_ops_incremental_name compatibility check
  char legacy_path[512];
  files_ops_incremental_name(test_temp_dir, "analizer", ".pcap", legacy_path);
  TEST_ASSERT_EQUAL_STRING(expected_path, legacy_path);
}

void test_incremental_name_traversal_blocked(void) {
  char out_path[256];

  // Traversal in base_name
  TEST_ASSERT_FALSE(files_ops_incremental_name_buf(
      test_temp_dir, "../../evil", ".pcap", out_path, sizeof(out_path)));
  TEST_ASSERT_EQUAL_STRING("", out_path);

  // Traversal in dir_path
  TEST_ASSERT_FALSE(files_ops_incremental_name_buf(
      "/tmp/../etc", "test", ".log", out_path, sizeof(out_path)));
  TEST_ASSERT_EQUAL_STRING("", out_path);

  // Traversal in extension
  TEST_ASSERT_FALSE(files_ops_incremental_name_buf(
      test_temp_dir, "test", "/../evil", out_path, sizeof(out_path)));
  TEST_ASSERT_EQUAL_STRING("", out_path);

  // Legacy function also safely aborts
  files_ops_incremental_name(test_temp_dir, "../../evil", ".pcap", out_path);
  TEST_ASSERT_EQUAL_STRING("", out_path);
}

void test_incremental_name_buffer_bounds(void) {
  char tiny_buf[10];
  // Buffer size 10 is too small for path
  TEST_ASSERT_FALSE(files_ops_incremental_name_buf(
      test_temp_dir, "analizer", ".pcap", tiny_buf, sizeof(tiny_buf)));
  TEST_ASSERT_EQUAL_STRING("", tiny_buf);
}

// ============================================================================
// Main Unity Test Runner
// ============================================================================

int main(void) {
  UNITY_BEGIN();

  // 1. Path traversal
  RUN_TEST(test_path_traversal_null_and_empty);
  RUN_TEST(test_path_traversal_direct_dotdot);
  RUN_TEST(test_path_traversal_backslashes);
  RUN_TEST(test_path_traversal_url_encoded);
  RUN_TEST(test_path_traversal_legitimate_paths);

  // 2. Filename validation
  RUN_TEST(test_filename_validation_valid);
  RUN_TEST(test_filename_validation_invalid);

  // 3. Filename sanitization
  RUN_TEST(test_filename_sanitization);

  // 4. Safe path joining
  RUN_TEST(test_join_path_normal);
  RUN_TEST(test_join_path_trailing_slash);
  RUN_TEST(test_join_path_no_extension);
  RUN_TEST(test_join_path_rejection);
  RUN_TEST(test_join_path_buffer_bounds);

  // 5. Size formatting
  RUN_TEST(test_format_size_buffer);
  RUN_TEST(test_format_size_allocated);
  RUN_TEST(test_format_size_buf_overflow_prevention);

  // 6. File sizing and descriptors
  RUN_TEST(test_get_file_size_null);
  RUN_TEST(test_get_file_size_nonexistent);
  RUN_TEST(test_get_file_size_traversal_rejection);
  RUN_TEST(test_get_file_size_with_real_file);

  // 7. Incremental names & existence
  RUN_TEST(test_exists_and_incremental_name);
  RUN_TEST(test_incremental_name_traversal_blocked);
  RUN_TEST(test_incremental_name_buffer_bounds);

  return UNITY_END();
}

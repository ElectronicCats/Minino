// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "surv_test.h"

#define TEST_MAC_FORMAT    "%02x:%02x:%02x:%02x:%02x:%02x"
#define TEST_EMPTY_MAC     "00:00:00:00:00:00"
#define MAX_MAC_TABLE_SIZE 100
#define MAC_TIMEOUT_SEC    30

typedef struct {
  uint8_t mac[6];
  uint32_t timestamp;
} test_mac_entry_t;

static test_mac_entry_t s_mac_table[MAX_MAC_TABLE_SIZE];
static uint16_t s_mac_table_count = 0;
static uint16_t s_mac_table_head = 0;

static void test_format_mac_address(const uint8_t* mac, char* buf, size_t len) {
  if (mac == NULL || buf == NULL || len < 18) {
    return;
  }
  snprintf(buf, len, TEST_MAC_FORMAT, mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
}

static uint16_t test_get_frequency(uint8_t primary) {
  if (primary == 14)
    return 2484;
  return 2412 + 5 * (primary - 1);
}

static bool test_is_mac_in_table(const uint8_t* mac, uint32_t current_time) {
  for (uint16_t i = 0; i < s_mac_table_count; i++) {
    if (memcmp(s_mac_table[i].mac, mac, 6) == 0) {
      if (current_time - s_mac_table[i].timestamp < MAC_TIMEOUT_SEC) {
        return true;
      } else {
        s_mac_table[i].timestamp = current_time;
        return false;
      }
    }
  }
  return false;
}

static void test_add_mac_to_table(const uint8_t* mac, uint32_t current_time) {
  for (uint16_t i = 0; i < s_mac_table_count; i++) {
    if (memcmp(s_mac_table[i].mac, mac, 6) == 0) {
      s_mac_table[i].timestamp = current_time;
      return;
    }
  }
  if (s_mac_table_count < MAX_MAC_TABLE_SIZE) {
    memcpy(s_mac_table[s_mac_table_count].mac, mac, 6);
    s_mac_table[s_mac_table_count].timestamp = current_time;
    s_mac_table_count++;
  } else {
    memcpy(s_mac_table[s_mac_table_head].mac, mac, 6);
    s_mac_table[s_mac_table_head].timestamp = current_time;
    s_mac_table_head = (s_mac_table_head + 1) % MAX_MAC_TABLE_SIZE;
  }
}

static void test_sd_card_build_full_path(const char* path,
                                         char* full_path,
                                         size_t max_len) {
  const char* mount_point = "/sdcard";
  if (path == NULL || full_path == NULL || max_len == 0) {
    return;
  }
  if (strstr(path, "..") != NULL || strchr(path, '\\') != NULL) {
    full_path[0] = '\0';
    return;
  }
  if (strncmp(path, mount_point, strlen(mount_point)) == 0) {
    snprintf(full_path, max_len, "%s", path);
  } else if (path[0] == '/') {
    snprintf(full_path, max_len, "%s%s", mount_point, path);
  } else {
    snprintf(full_path, max_len, "%s/%s", mount_point, path);
  }
}

TEST_CASE("formateo seguro de MAC en stack sin asignacion dinamica",
          "[wardriving]") {
  uint8_t mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
  char buf[18];
  test_format_mac_address(mac, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("12:34:56:78:9a:bc", buf);
}

TEST_CASE("deteccion de MAC vacia 00:00:00:00:00:00", "[wardriving]") {
  uint8_t mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  char buf[18];
  test_format_mac_address(mac, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING(TEST_EMPTY_MAC, buf);
}

TEST_CASE("calculo de frecuencia por canal primario WiFi", "[wardriving]") {
  TEST_ASSERT_EQUAL_UINT16(2412, test_get_frequency(1));
  TEST_ASSERT_EQUAL_UINT16(2437, test_get_frequency(6));
  TEST_ASSERT_EQUAL_UINT16(2462, test_get_frequency(11));
  TEST_ASSERT_EQUAL_UINT16(2472, test_get_frequency(13));
  TEST_ASSERT_EQUAL_UINT16(2484, test_get_frequency(14));
}

TEST_CASE("deduplicacion y expiracion de MAC en tabla de escaneo",
          "[wardriving]") {
  s_mac_table_count = 0;
  s_mac_table_head = 0;

  uint8_t mac1[6] = {0xaa, 0xbb, 0xcc, 0x11, 0x22, 0x33};
  uint8_t mac2[6] = {0xaa, 0xbb, 0xcc, 0x11, 0x22, 0x44};

  TEST_ASSERT_FALSE(test_is_mac_in_table(mac1, 100));
  test_add_mac_to_table(mac1, 100);

  // Dentro de ventana de timeout (30s) -> detecta duplicado
  TEST_ASSERT_TRUE(test_is_mac_in_table(mac1, 110));
  TEST_ASSERT_FALSE(test_is_mac_in_table(mac2, 110));

  // Fuera de ventana de timeout (30s) -> expira y retorna false
  TEST_ASSERT_FALSE(test_is_mac_in_table(mac1, 135));
}

TEST_CASE("manejo circular de desbordamiento en tabla de MAC", "[wardriving]") {
  s_mac_table_count = 0;
  s_mac_table_head = 0;

  for (int i = 0; i < MAX_MAC_TABLE_SIZE + 20; i++) {
    uint8_t mac[6] = {
        0x02, 0x00, 0x00, (uint8_t) (i >> 8), (uint8_t) (i & 0xFF), 0x01};
    test_add_mac_to_table(mac, 100 + i);
  }

  TEST_ASSERT_EQUAL_UINT16(MAX_MAC_TABLE_SIZE, s_mac_table_count);
  TEST_ASSERT_EQUAL_UINT16(20 % MAX_MAC_TABLE_SIZE, s_mac_table_head);
}

TEST_CASE("resolucion de rutas hacia SD y prevencion de path traversal",
          "[wardriving]") {
  char path_out[128];

  // Ruta relativa -> /sdcard/warfi
  test_sd_card_build_full_path("warfi", path_out, sizeof(path_out));
  TEST_ASSERT_EQUAL_STRING("/sdcard/warfi", path_out);

  // Ruta absoluta sin prefijo /sdcard -> /sdcard/warfi/Warfi.csv
  test_sd_card_build_full_path("/warfi/Warfi.csv", path_out, sizeof(path_out));
  TEST_ASSERT_EQUAL_STRING("/sdcard/warfi/Warfi.csv", path_out);

  // Ruta absoluta con prefijo /sdcard -> sin duplicar prefijo
  test_sd_card_build_full_path("/sdcard/apps/analizer/pcaps/analizer00.pcap",
                               path_out, sizeof(path_out));
  TEST_ASSERT_EQUAL_STRING("/sdcard/apps/analizer/pcaps/analizer00.pcap",
                           path_out);

  // Ataque de path traversal prevenido
  test_sd_card_build_full_path("../../etc/passwd", path_out, sizeof(path_out));
  TEST_ASSERT_EQUAL_STRING("", path_out);
}

TEST_CASE("formateo de linea CSV de Wigle sin desbordamiento de buffer",
          "[wardriving]") {
  char csv_line[150];
  const char* mac = "aa:bb:cc:dd:ee:ff";
  const char* ssid = "TestingNetwork_1234567890";
  const char* auth = "WPA2_PSK";
  const char* dt = "2026-09-10 12:00:00";
  int ch = 6;
  unsigned freq = 2437;
  int rssi = -65;
  double lat = 20.644129;
  double lon = -100.461815;
  double alt = 1892.85;
  double acc = 1.5;

  int written = snprintf(
      csv_line, sizeof(csv_line), "%s,%s,%s,%s,%d,%u,%d,%f,%f,%f,%f,%s,%s,%s\n",
      mac, ssid, auth, dt, ch, freq, rssi, lat, lon, alt, acc, "", "", "WIFI");

  TEST_ASSERT_GREATER_THAN(0, written);
  TEST_ASSERT_LESS_THAN(sizeof(csv_line), (size_t) written);
  TEST_ASSERT_NOT_NULL(strstr(csv_line, "aa:bb:cc:dd:ee:ff"));
  TEST_ASSERT_NOT_NULL(strstr(csv_line, "WIFI"));
}

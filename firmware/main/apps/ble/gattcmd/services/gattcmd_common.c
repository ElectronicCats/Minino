#include <string.h>
#include "services/gattcmd_service.h"

void parse_address_colon(const char* str, uint8_t addr[6]) {
  sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &addr[0], &addr[1], &addr[2],
         &addr[3], &addr[4], &addr[5]);
}

void parse_address_raw(const char* str, uint8_t addr[6]) {
  for (int i = 0; i < 6; i++) {
    sscanf(str + 2 * i, "%2hhx", &addr[i]);
  }
}

uint16_t hex_string_to_uint16(const char* hex_str) {
  return (uint16_t) strtol(hex_str, NULL, 16);
}

int hex_string_to_bytes(const char* hex_str, uint8_t* out_buf, size_t max_len) {
  size_t len = strlen(hex_str);
  if (len % 2 != 0 || len / 2 > max_len) {
    return -1;
  }

  for (size_t i = 0; i < len / 2; ++i) {
    char byte_str[3] = {hex_str[2 * i], hex_str[2 * i + 1], '\0'};
    out_buf[i] = (uint8_t) strtol(byte_str, NULL, 16);
  }
  return (int) (len / 2);
}
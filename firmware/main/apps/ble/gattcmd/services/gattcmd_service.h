#ifndef __GATTCMD_ENUM_H
#define __GATTCMD_ENUM_H

#include <stdint.h>
#include <stdio.h>

#define GATTCMD_ENUM_TAG     "GATT_ENUM"
#define GATTCMD_ENUM_PROFILE 1
#define GATTCMD_ENUM_APP_ID  0
#define INVALID_HANDLE       0

void gattcmd_enum_begin(char* saddress);
void gattcmd_write_begin(char* saddress, uint16_t target_uuid, char* value_str);

void gattcmd_scan_begin();

void gattcmd_scan_stop();
void gattcmd_enum_stop();
void gattcmd_write_stop();

void parse_address_colon(const char* str, uint8_t addr[6]);
void parse_address_raw(const char* str, uint8_t addr[6]);
uint16_t hex_string_to_uint16(const char* hex_str);
int hex_string_to_bytes(const char* hex_str, uint8_t* out_buf, size_t max_len);

#endif
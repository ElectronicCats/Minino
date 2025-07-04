#ifndef __GATTCMD_ENUM_H
#define __GATTCMD_ENUM_H

#include <stdint.h>

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
#endif
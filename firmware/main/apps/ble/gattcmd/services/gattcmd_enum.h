#ifndef __GATTCMD_ENUM_H
#define __GATTCMD_ENUM_H

#define GATTCMD_ENUM_TAG     "GATT_ENUM"
#define GATTCMD_ENUM_PROFILE 1
#define GATTCMD_ENUM_APP_ID  0
#define INVALID_HANDLE       0

void gattcmd_enum_begin(char* saddress);
#endif
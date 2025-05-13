#ifndef __GATTCMD_MODULE_H
  #define __GATTCMD_MODULE_H
  #include <stdint.h>

void gattcmd_begin(void);
void gattcmd_module_set_remote_address(char* saddress);
void gattcmd_module_gatt_write(char* gatt, char* value);
void gattcmd_module_connect();
void gattcmd_module_disconnect();

void gattcmd_module_enum_client(char* saddress);

#endif

// gattcmd_set_client be:96:24:00:07:da
// gattcmd_connect
// gattcmd_write fff3 7e0404000000ff00ef
// gattcmd_enum be:96:24:00:07:da
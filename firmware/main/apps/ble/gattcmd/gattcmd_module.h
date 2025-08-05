#ifndef __GATTCMD_MODULE_H
#define __GATTCMD_MODULE_H
#include <stdint.h>

void gattcmd_begin(void);
void gattcmd_module_set_remote_address(char* saddress);
void gattcmd_module_gatt_write(char* saddress, char* gatt, char* value);

void gattcmd_module_enum_client(char* saddress);
void gattcmd_module_scan_client();

void gattcmd_module_recon(const char* bt_addr);

void gattcmd_module_stop_workers();

#endif
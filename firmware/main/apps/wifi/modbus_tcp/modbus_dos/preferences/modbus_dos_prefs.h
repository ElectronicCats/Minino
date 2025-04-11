#ifndef _MODBUS_DOS_PREFS_H_
#define _MODBUS_DOS_PREFS_H_

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  char* ssid;
  char* pass;
  char* ip;
  uint16_t port;
} modbus_dos_prefs_t;

void modbus_dos_prefs_begin();
bool modbus_dos_prefs_check();
void modbus_dos_prefs_set_ssid(char* ssid);
void modbus_dos_prefs_set_pass(char* pass);
void modbus_dos_prefs_set_ip(char* ip);
void modbus_dos_prefs_set_port(uint16_t port);
void modbus_dos_prefs_print_prefs();

modbus_dos_prefs_t* modubs_dos_prefs_get_prefs();

#endif  // _MODBUS_DOS_PREFS_H_
#ifndef _MODBUS_TCP_PREFS_H_
#define _MODBUS_TCP_PREFS_H_

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  uint8_t request[260];
  size_t request_len;
  char* ip;
  int port;
  bool ip_set;
} modbus_tcp_prefs_t;

void modbus_tcp_prefs_begin();
void modbus_tcp_prefs_set_req(uint8_t* request, size_t request_len);
void modbus_tcp_prefs_set_server(char* ip, int port);
void modbus_tcp_prefs_print_prefs();

modbus_tcp_prefs_t* modubs_tcp_prefs_get_prefs();

#endif  // _MODBUS_TCP_PREFS_H_
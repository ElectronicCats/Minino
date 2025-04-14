#ifndef _MODBUS_ENGINE_H_
#define _MODBUS_ENGINE_H_

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  uint8_t request[260];
  size_t request_len;
  char* ip;
  char* port;
  int sock;
  bool wifi_connected;
} modbus_engine_t;

void modbus_engine_begin();

void modbus_engine_set_server(char* ip, int port);
int modbus_engine_connect();
void modbus_engine_disconnect();

void modbus_engine_set_request(uint8_t* request, size_t request_len);
void modbus_engine_send_request();
modbus_engine_t* modbus_engine_get_ctx();

#endif  //_MODBUS_ENGINE_H_
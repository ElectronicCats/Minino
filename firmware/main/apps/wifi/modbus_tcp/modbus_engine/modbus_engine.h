#ifndef _MODBUS_ENGINE_H_
#define _MODBUS_ENGINE_H_

#include <stdbool.h>
#include <stdio.h>

/* Capacity of every Modbus/TCP request buffer in this app (engine, prefs and
   the two generators). It has to be named because the value is used both to
   size the arrays and to bound the copies: a generator once wrote 269 bytes
   into a 260-byte stack buffer, and the oversized length was then persisted to
   NVS and re-read on every boot. */
#define MODBUS_TCP_MAX_REQUEST_LEN 260

typedef struct {
  uint8_t request[MODBUS_TCP_MAX_REQUEST_LEN];
  size_t request_len;
  char* ip;
  int port;
  int sock;
  bool wifi_connected;
  bool ip_set;
} modbus_engine_t;

void modbus_engine_begin();

void modbus_engine_set_server(char* ip, int port);
int modbus_engine_connect();
void modbus_engine_disconnect();

void modbus_engine_set_request(uint8_t* request, size_t request_len);
void modbus_engine_send_request();
modbus_engine_t* modbus_engine_get_ctx();
void modbus_engine_print_status();

#endif  //_MODBUS_ENGINE_H_
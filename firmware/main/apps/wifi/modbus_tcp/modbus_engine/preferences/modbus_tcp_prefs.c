#include "modbus_tcp_prefs.h"

#include <string.h>
#include "esp_log.h"

#include "preferences.h"

#define MODBUS_TCP_REQ_MEM         "MBTCPREQ"
#define MODBUS_TCP_LEN_MEM         "MBTCPLEN"
#define MODBUS_TCP_SERVER_IP_MEM   "MBTCPIP"
#define MODBUS_TCP_SERVER_PORT_MEM "MBTCPPORT"

#define MODBUS_TCP_PORT_DEF 502

#define IP_MAX_LEGHT 16

static const char* TAG = "modbus_tcp_prefs";

static modbus_tcp_prefs_t* mb_tcp_prefs = NULL;

void modbus_tcp_prefs_begin();

static void load_prefs() {
  if (!mb_tcp_prefs) {
    modbus_tcp_prefs_begin();
  }

  mb_tcp_prefs->request_len = preferences_get_uint(MODBUS_TCP_LEN_MEM, 0);
  preferences_get_bytes(MODBUS_TCP_REQ_MEM, mb_tcp_prefs->request,
                        mb_tcp_prefs->request_len);

  mb_tcp_prefs->port =
      preferences_get_int(MODBUS_TCP_SERVER_PORT_MEM, MODBUS_TCP_PORT_DEF);
  preferences_get_string(MODBUS_TCP_SERVER_IP_MEM, mb_tcp_prefs->ip,
                         IP_MAX_LEGHT);
}

static void save_prefs() {
  preferences_put_bytes(MODBUS_TCP_REQ_MEM, mb_tcp_prefs->request,
                        mb_tcp_prefs->request_len);
  preferences_put_uint(MODBUS_TCP_LEN_MEM, mb_tcp_prefs->request_len);
  preferences_put_string(MODBUS_TCP_SERVER_IP_MEM, mb_tcp_prefs->ip);
  preferences_put_int(MODBUS_TCP_SERVER_PORT_MEM, mb_tcp_prefs->port);
}

void modbus_tcp_prefs_begin() {
  mb_tcp_prefs = calloc(1, sizeof(modbus_tcp_prefs_t));
  mb_tcp_prefs->ip = malloc(IP_MAX_LEGHT);
  load_prefs();
}

modbus_tcp_prefs_t* modubs_tcp_prefs_get_prefs() {
  return mb_tcp_prefs;
}

void modbus_tcp_prefs_set_req(uint8_t* request, size_t request_len) {
  if (!mb_tcp_prefs) {
    modbus_tcp_prefs_begin();
  }

  memcpy(mb_tcp_prefs->request, request, request_len);
  mb_tcp_prefs->request_len = request_len;

  save_prefs();
}
void modbus_tcp_prefs_set_server(char* ip, int port) {
  if (!mb_tcp_prefs) {
    modbus_tcp_prefs_begin();
  }

  strncpy(mb_tcp_prefs->ip, ip, IP_MAX_LEGHT - 1);
  mb_tcp_prefs->ip[IP_MAX_LEGHT - 1] = '\0';
  mb_tcp_prefs->port = port;

  save_prefs();
}

void modbus_tcp_prefs_print_prefs() {
  if (!mb_tcp_prefs) {
    modbus_tcp_prefs_begin();
  }

  ESP_LOGI(TAG, "Reques:");
  for (int i = 0; i < mb_tcp_prefs->request_len; i++) {
    printf("%02X ", mb_tcp_prefs->request[i]);
  }

  ESP_LOGI(TAG, "Req: %s", mb_tcp_prefs->request);
  ESP_LOGI(TAG, "Req Len: %d", mb_tcp_prefs->request_len);
  ESP_LOGI(TAG, "IP: %s", mb_tcp_prefs->ip);
  ESP_LOGI(TAG, "Port: %d", mb_tcp_prefs->port);
}
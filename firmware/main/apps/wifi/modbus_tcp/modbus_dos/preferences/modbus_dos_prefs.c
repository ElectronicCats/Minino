#include "modbus_dos_prefs.h"

#include <string.h>
#include "esp_log.h"

#include "preferences.h"

#define MODBUS_DOS_MAGIC_MEM "MBDOSMAGIC"
#define MODBUS_DOS_MAGIC_NUM 57

#define MODBUS_DOS_SSID_MEM       "MBDOSSSID"
#define MODBUS_DOS_PASS_MEM       "MBDOSPASS"
#define MODBUS_DOS_SLAVE_IP_MEM   "MBDOSSIP"
#define MODBUS_DOS_SLAVE_PORT_MEM "MBDOSSP"

#define MODBUS_DOS_SSID_DEF "ssid"
#define MODBUS_DOS_PASS_DEF "pass"
#define MODBUS_DOS_IP_DEF   "192.168.1.100"
#define MODBUS_DOS_PORT_DEF 502

#define SSID_MAX_LENGTH 33
#define PASS_MAX_LEGHT  64
#define IP_MAX_LEGHT    16

static const char* TAG = "modbus_dos_prefs";

static modbus_dos_prefs_t* mb_dos_prefs = NULL;

void modbus_dos_prefs_begin();
bool modbus_dos_prefs_check();

static void load_prefs() {
  if (!mb_dos_prefs) {
    modbus_dos_prefs_begin();
  }

  mb_dos_prefs->port =
      preferences_get_ushort(MODBUS_DOS_SLAVE_PORT_MEM, MODBUS_DOS_PORT_DEF);

  if (modbus_dos_prefs_check()) {
    preferences_get_string(MODBUS_DOS_SSID_MEM, mb_dos_prefs->ssid,
                           SSID_MAX_LENGTH);
    preferences_get_string(MODBUS_DOS_PASS_MEM, mb_dos_prefs->pass,
                           PASS_MAX_LEGHT);
    preferences_get_string(MODBUS_DOS_SLAVE_IP_MEM, mb_dos_prefs->ip,
                           IP_MAX_LEGHT);
  }
}

static void save_prefs() {
  preferences_put_string(MODBUS_DOS_SSID_MEM, mb_dos_prefs->ssid);
  preferences_put_string(MODBUS_DOS_PASS_MEM, mb_dos_prefs->pass);
  preferences_put_string(MODBUS_DOS_SLAVE_IP_MEM, mb_dos_prefs->ip);
  preferences_put_ushort(MODBUS_DOS_SLAVE_PORT_MEM, mb_dos_prefs->port);
  preferences_put_char(MODBUS_DOS_MAGIC_MEM, MODBUS_DOS_MAGIC_NUM);
}

void modbus_dos_prefs_begin() {
  mb_dos_prefs = calloc(1, sizeof(modbus_dos_prefs_t));
  mb_dos_prefs->ssid = malloc(SSID_MAX_LENGTH);
  mb_dos_prefs->pass = malloc(PASS_MAX_LEGHT);
  mb_dos_prefs->ip = malloc(IP_MAX_LEGHT);
  load_prefs();
}

bool modbus_dos_prefs_check() {
  return preferences_get_char(MODBUS_DOS_MAGIC_MEM, 0) == MODBUS_DOS_MAGIC_NUM;
}

modbus_dos_prefs_t* modubs_dos_prefs_get_prefs() {
  return mb_dos_prefs;
}

void modbus_dos_prefs_set_ssid(char* ssid) {
  if (!mb_dos_prefs) {
    modbus_dos_prefs_begin();
  }
  strncpy(mb_dos_prefs->ssid, ssid, SSID_MAX_LENGTH - 1);
  mb_dos_prefs->ssid[SSID_MAX_LENGTH - 1] = '\0';

  save_prefs();
}
void modbus_dos_prefs_set_pass(char* pass) {
  if (!mb_dos_prefs) {
    modbus_dos_prefs_begin();
  }
  strncpy(mb_dos_prefs->pass, pass, PASS_MAX_LEGHT - 1);
  mb_dos_prefs->pass[PASS_MAX_LEGHT - 1] = '\0';
  save_prefs();
}
void modbus_dos_prefs_set_ip(char* ip) {
  if (!mb_dos_prefs) {
    modbus_dos_prefs_begin();
  }
  strncpy(mb_dos_prefs->ip, ip, IP_MAX_LEGHT - 1);
  mb_dos_prefs->ip[IP_MAX_LEGHT - 1] = '\0';
  save_prefs();
}

void modbus_dos_prefs_set_port(uint16_t port) {
  if (!mb_dos_prefs) {
    modbus_dos_prefs_begin();
  }
  mb_dos_prefs->port = port;
  save_prefs();
}

void modbus_dos_prefs_print_prefs() {
  if (!mb_dos_prefs) {
    modbus_dos_prefs_begin();
  }
  ESP_LOGI(TAG, "SSID: %s", mb_dos_prefs->ssid);
  ESP_LOGI(TAG, "PASS: %s", mb_dos_prefs->pass);
  ESP_LOGI(TAG, "IP: %s", mb_dos_prefs->ip);
  ESP_LOGI(TAG, "Port: %d", mb_dos_prefs->port);
}
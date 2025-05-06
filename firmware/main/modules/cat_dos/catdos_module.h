#pragma once

#include <stdint.h>
#ifndef CATDOS_MODULE_H
  #define CATDOS_MODULE_H

typedef enum {
  CATDOS_MENU_AP = 0,
  CATDOS_MENU_TARGET,
  CATDOS_MENU_ATTACK,
  CATDOS_MENU_HELP,
} catdos_menu_t;

void catdos_module_begin(void);

void catdos_module_set_config(char* ssid, char* passwd);
void catdos_module_set_target(char* host, char* port, char* endpoint);
void catdos_module_send_attack();
#endif  // CATDOS_MODULE_H

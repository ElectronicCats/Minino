#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef int menu_idx_t;

void menus_module_set_menu_over_cmd(char *cmd);
bool menus_module_get_app_state(void);
void menus_module_exit_app(void);
void menus_module_refresh(void);
menu_idx_t menus_module_get_current_menu(void);

#include "i2c_scanner.h"

#include <stdio.h>
#include <stdlib.h>  // Para malloc y realloc
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_system.h"
#include "general_submenu.h"
#include "keyboard_module.h"
#include "menus_module.h"

#define I2C_MASTER_NUM I2C_NUM_0

static char** nodes_arr = NULL;

static void i2c_scanner(uint8_t** found_addresses, size_t* num_addresses) {
  *num_addresses = 0;

  for (uint8_t address = 1; address < 127; address++) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    esp_err_t err =
        i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (err == ESP_OK) {
      *found_addresses =
          realloc(*found_addresses, (*num_addresses + 1) * sizeof(uint8_t));
      if (*found_addresses == NULL) {
        return;
      }
      (*found_addresses)[*num_addresses] = address;
      (*num_addresses)++;
    }
  }
}

void imprimir_direcciones(uint8_t* found_addresses, size_t num_addresses) {
  if (num_addresses == 0) {
  } else {
    nodes_arr = malloc(sizeof(char*) * num_addresses);
    for (size_t i = 0; i < num_addresses; i++) {
      nodes_arr[i] = malloc(20);
      sprintf(nodes_arr[i], "%s: 0x%02X",
              found_addresses[i] == 0x3C ? "(Screen)" : "Node",
              found_addresses[i]);
    }
  }
}

static void show_scan_result(size_t num_nodes) {
  general_submenu_menu_t result;
  result.options = nodes_arr;
  result.options_count = num_nodes;
  result.exit_cb = menus_module_restart;
  general_submenu(result);
}

static void i2c_scanner_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      menus_module_restart();
      break;
    default:
      break;
  }
}

void i2c_scanner_begin() {
  uint8_t* found_addresses = NULL;
  size_t num_addresses = 0;

  menus_module_set_app_state(true, i2c_scanner_input_cb);
  i2c_scanner(&found_addresses, &num_addresses);
  imprimir_direcciones(found_addresses, num_addresses);
  free(found_addresses);
  show_scan_result(num_addresses);
}

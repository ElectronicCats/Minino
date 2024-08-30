#include <stdbool.h>
#include <stdint.h>
#include "general/general_screens.h"
#include "radio_selector.h"
#include "zigbee_switch.h"
#ifndef Z_SWITCH_SCREENS_H
  #define Z_SWITCH_SCREENS_H

void z_switch_handle_connection_status(uint8_t status);
#endif  // Z_SWITCH_SCREENS_H

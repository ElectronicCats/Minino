#ifndef _MODBUS_ATTACKS_H_
#define _MODBUS_ATTACKS_H_

#include <stdbool.h>
#include <stdio.h>

typedef void (*connection_callback)(bool state);

void modbus_attacks_writer(connection_callback cb);
void modbus_attacks_stop();

#endif  // _MODBUS_ATTACKS_H_
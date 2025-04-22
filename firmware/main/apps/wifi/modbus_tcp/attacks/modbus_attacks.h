#ifndef _MODBUS_ATTACKS_H_
#define _MODBUS_ATTACKS_H_

#include <stdbool.h>
#include <stdio.h>

typedef void (*connection_callback)(bool state);

bool modbus_attacks_writer_single();
void modbus_attacks_dos(connection_callback cb);
void modbus_attacks_writer(connection_callback cb);
void modbus_attacks_stop();

#endif  // _MODBUS_ATTACKS_H_
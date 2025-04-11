#ifndef _MODBUS_DOS_H_
#define _MODBUS_DOS_H_

#include <stdio.h>

void modbus_dos_begin();
void modbus_dos_stop();
void modbus_dos_send_pkt(uint8_t* pkt, size_t pkt_len);

#endif  // _MODBUS_DOS_H_
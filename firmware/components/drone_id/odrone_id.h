#ifndef _ODRONE_ID_H_
#define _ODRONE_ID_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void odrone_id_begin(uint8_t num_drones,
                     uint8_t channel,
                     float latitude,
                     float longitude);

#ifdef __cplusplus
}
#endif

#endif  // _ODRONE_ID_H_
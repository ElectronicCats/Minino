// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void zigbee_mesh_mapper_begin(void);
void zigbee_mesh_mapper_stop(void);
void zigbee_mesh_mapper_on_exit(void);

#ifdef __cplusplus
}
#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SURV_SIM_ALL = 0,
  SURV_SIM_FLOCK,
  SURV_SIM_AIRTAG,
  SURV_SIM_AXON,
  SURV_SIM_SKIMMER,
} surv_sim_mode_t;

void surv_sim_begin_all(void);
void surv_sim_begin_flock(void);
void surv_sim_begin_airtag(void);
void surv_sim_begin_axon(void);
void surv_sim_begin_skimmer(void);
void surv_sim_stop(void);

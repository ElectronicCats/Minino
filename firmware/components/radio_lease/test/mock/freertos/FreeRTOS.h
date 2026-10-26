/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

/* Host stand-in: the tests are single threaded, so the critical sections only
   have to compile. */
typedef unsigned portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0U
#define portENTER_CRITICAL(mux)      ((void) (*(mux) += 1))
#define portEXIT_CRITICAL(mux)       ((void) (*(mux) -= 1))

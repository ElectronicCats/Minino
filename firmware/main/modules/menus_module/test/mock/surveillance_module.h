/* Host-test stub for surveillance_module.h. */
#pragma once
#include "menu_callbacks.h"

/* The catalogue uses these two, so the stub has to declare them: a
   definition in menu_callbacks_stub.c without a declaration here leaves the
   catalogue with an undefined reference. */
void surveillance_module_begin_flock(void);
void surveillance_module_stop(void);

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ctrl_c_callback_t)(void);

// Register all system functions
void cat_console_begin(void);
void show_dos_commands();
void cat_console_register_ctrl_c_handler(ctrl_c_callback_t callback);

// Console pause/resume functions
void cat_console_pause(void);
void cat_console_resume(void);
bool cat_console_is_paused(void);

#ifdef __cplusplus
}
#endif

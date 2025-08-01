#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ctrl_c_callback_t)(void);

// Register all system functions
void cat_console_begin(void);
void show_dos_commands();
void cat_console_register_ctrl_c_handler(ctrl_c_callback_t callback);
#ifdef __cplusplus
}
#endif

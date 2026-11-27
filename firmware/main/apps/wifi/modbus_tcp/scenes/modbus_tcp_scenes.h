#ifndef _MODBUS_TCP_SCENES_H_
#define _MODBUS_TCP_SCENES_H_

#include <stdio.h>

void modbus_tcp_scenes_main();
void modbus_tcp_scenes_settings();
void modbus_tcp_scenes_exit(void);

/* Menu entry point: enters through app_manager so that exiting runs the app's
   on_exit cleanup and returns to the parent menu. Calling
   modbus_tcp_scenes_main directly leaves app_manager with no active app, so the
   back button becomes a no-op and the UI freezes. */
void app_manager_enter_wrapper_modbus_tcp(void);

#endif  // _MODBUS_tcp_SCENES_H_
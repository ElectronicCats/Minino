### Add new menu

#### Menus structure

TODO

#### Steps to add new menu

TODO

##### Configuration menu

You can add a configuration menu inside an application menu or the `Settings` menu. In this guide, we will show you how to add a configuration under the `Settings` menu. Let's take the logs configuration as an example. The look will be similar to the following in the Minino UI:

```
[x] USB
[ ] UART TXD/RXD
```

1. Create a new field in the `menu_idx_t` enum in the `menus.h` file:

```c
typedef enum {
  ...
  MENU_SETTINGS_LOGS_OUTPUT,
  ...
} menu_idx_t;
```

2. Add the new menu to the `menus[]` array in `menus.h`:

```c
{ .display_name = "Logs Output",
  .menu_idx = MENU_SETTINGS_LOGS_OUTPUT,
  .parent_idx = MENU_SETTINGS_SYSTEM,
  .last_selected_submenu = 0,
  .on_enter_cb = NULL,
  .on_exit_cb = NULL,
  .is_visible = true},
```

You can compile to verify that the new menu is visible at this point.

3. Create a module to handle the new menu behavior. In this case, we will create two files, `logs_output.h` and `logs_output.c` in the path `main/settings/uart_bridge`. The header file will only contain the `logs_output` function declaration with the following signature:

```c
#pragma once

void logs_output();
```

4. Implement the logic in the `logs_output.c` file:

```c
#include "logs_output.h"
#include "esp_log.h"
#include "general_radio_selection.h"
#include "menus_module.h"
#include "preferences.h"
#include "uart_bridge.h"

static char* logs_output_options[] = {
    "USB",
    "UART TXD/RXD",
};

// Same order as logs_output_options
typedef enum {
  USB,
  UART,
} logs_output_option_t;

uint8_t logs_output_get_option() {
  return preferences_get_uchar("logs_output", USB);
}

void logs_output_set_logs_output(logs_output_option_t selected_option) {
  switch (selected_option) {
    case USB:
      uart_bridge_set_logs_to_usb();
      preferences_put_uchar("logs_output", USB);
      break;
    case UART:
      uart_bridge_set_logs_to_uart();
      preferences_put_uchar("logs_output", UART);
      break;
    default:
      break;
  }
}

void logs_output() {
  general_radio_selection_menu_t logs_output_menu = {
      .options = logs_output_options,
      .banner = "Logs Output",
      .current_option = logs_output_get_option(),
      .options_count = sizeof(logs_output_options) / sizeof(char*),
      .select_cb = logs_output_set_logs_output,
      .exit_cb = menus_module_exit_app,
      .style = RADIO_SELECTION_OLD_STYLE,
  };

  general_radio_selection(logs_output_menu);
}
```

As we can see, the `logs_output` function will display a radio selection menu with two options: `USB` and `UART TXD/RXD`. The `logs_output_get_option` function will return the current selected option, and the `logs_output_set_logs_output` function will set the logs output to the selected option.

> [!IMPORTANT]
> It's important to save the selected option in the preferences to keep the selected option after a reboot.

5. Include the header file in the `menu.h` and add the `logs_output` function to the `on_enter_cb` field of the `MENU_SETTINGS_LOGS_OUTPUT` menu:

```c
...
#include "logs_output.h"
...

{ .display_name = "Logs Output",
  .menu_idx = MENU_SETTINGS_LOGS_OUTPUT,
  .parent_idx = MENU_SETTINGS_SYSTEM,
  .last_selected_submenu = 0,
  .on_enter_cb = logs_output,
  .on_exit_cb = NULL,
  .is_visible = true},
```

6. And that's it! You can now test the new menu in the Minino UI.

### Add a component

To add a new component, follow these steps:

1. Create a component under the `components` directory using the following command:

```bash
idf.py create-component -C components/ <component_name>
```

2. Add the component to the `CMakeLists.txt` file in the `main` directory:

```cmake
set(EXTRA_COMPONENT_DIRS components/<component_name>)
```

Your component is now ready to be used in the project.

### Add configuration to enable/disable ESP_LOGs of a specific component/module

1. Add a new configuration option in the `Kconfig.projbuild` file in the `minino_config` component. For example, for the `uart_bridge` component, add the following:

```cmake
config UART_BRIDGE_DEBUG
			bool "UART Bridge debug"
			default false
			help
				Enable UART Bridge debug.
```

> [!IMPORTANT]
> Add the configuration using alphabetical order.

> Note: Make sure to include the configuration under the right section, for now, there are two sections: `"Modules debug configuration"` `"Components debug configuration"`.

2. Make sure your configuration is visible in the `menuconfig`, run the following command and go to `General project` -> `Debug configuration`:

```bash
idf.py menuconfig
```

3. Add the following instruction to the `begin` function or the function where your component/module is initialized:

```c
#if !defined(CONFIG_MODULE_NAME_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
```

For example, for the `uart_bridge` component, add the following to the `uart_bridge.c` file:

```c
static const char* TAG = "uart_bridge";

esp_err_t uart_bridge_begin(int baud_rate, int buffer_size) {
#if !defined(CONFIG_UART_BRIDGE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  ...
}
```

4. You can now enable/disable the logs of the component/module using the `menuconfig`.
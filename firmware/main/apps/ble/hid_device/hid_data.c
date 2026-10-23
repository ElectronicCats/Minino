#include "hid_screens.h"

/* Definitions split out of hid_screens.h: the tables used to be defined in the
   header, so every translation unit that included it emitted its own copy and
   the linker only accepted it thanks to -Wl,-zmuldefs. */

char* hid_menu_items[HID_MENU_COUNT] = {"Device name", "Device MAC", "Start"};

char* hid_device_items[HID_DEVICE_COUNT] = {
    "Volume Up", "Volume Down", "Play",       "Pause",
    "Stop",      "Mute",        "Next Track", "Previous Track"};
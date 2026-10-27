#include "captive_screens.h"

/* Definitions split out of captive_screens.h: the tables used to be defined in
   the header, so every translation unit that included it emitted its own copy
   and the linker only accepted it thanks to -Wl,-zmuldefs. */

const char* captive_main_menu[] = {"Portals", "Redirect", "Mode", "Preferences",
                                   "Channel", "Run",      "Help"};

const char* no_folder_help[] = {"If you dont have",
                                "the Portals",
                                "folder already",
                                "in your memory",
                                "create",
                                "a folder.",
                                "called:",
                                "",
                                "portals",
                                "",
                                "in the root",
                                "path of the",
                                "SD card and",
                                "save the",
                                "HTML files",
                                "inside",
                                "",
                                "use the user",
                                "inputs to ",
                                "handle the",
                                "data",
                                "",
                                "configure the",
                                "preference",
                                "dump mode",
                                "",
                                "If you",
                                "have problems",
                                "with the AP",
                                "change the",
                                "channel."};
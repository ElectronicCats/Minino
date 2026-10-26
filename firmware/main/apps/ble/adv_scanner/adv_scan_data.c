#include "adv_scan_screens.h"

/* Definitions split out of adv_scan_screens.h: the tables used to be defined in
   the header, so every translation unit that included it emitted its own copy
   and the linker only accepted it thanks to -Wl,-zmuldefs. */

char* scan_menu_items[SCAN_MENU_COUNT] = {"Scan Type", "ADV Filter", "Start"};

char* scan_type_items[2] = {"Active", "Passive"};
char* scan_filter_items[4] = {"Allow All", "Allow Only WLST", "Allow UND RPA",
                              "Allow WLST & RPA"};
char* evt_adv_type[5] = {"IND", "DIRECT_IND", "SCAN_IND", "NONCONN_IND",
                         "SCAN_RSP"};
#ifndef CAPTIVE_MODULE_H
#define CAPTIVE_MODULE_H

#define CAPTIVE_PORTAL_PATH_NAME       "portals"
#define CAPTIVE_PORTAL_DEFAULT_NAME    "root.html"
#define CAPTIVE_PORTAL_LIMIT_PORTALS   20
#define CAPTIVE_PORTAL_MAX_DEFAULT_LEN 24
#define CAPTIVE_PORTAL_MODE_FS_KEY     "cpmode"
#define CAPTIVE_PORTAL_FS_KEY          "cpportal"
#define CAPTIVE_PORTAL_NET_NAME        "WIFI_AP_DEF"

// USER INputs
#define CAPTIVE_USER_INPUT1 "user1"
#define CAPTIVE_USER_INPUT2 "user2"
#define CAPTIVE_USER_INPUT3 "user3"
#define CAPTIVE_USER_INPUT4 "user4"

void captive_module_main(void);

#endif
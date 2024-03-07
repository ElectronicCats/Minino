#ifndef DISPLAY_HELPER_H
#define DISPLAY_HELPER_H

enum MenuLayer {
    LAYER_MAIN_MENU = 0,
    LAYER_APPLICATIONS,
    LAYER_SETTINGS,
    LAYER_ABOUT,
};

enum MainMenuItem {
    MAIN_MENU_APPLICATIONS = 0,
    MAIN_MENU_SETTINGS,
    MAIN_MENU_ABOUT,
};

typedef enum MenuLayer Layer;

static char* mainOptions[] = {
    "Applications",
    "Settings",
    "About",
};

#endif // DISPLAY_HELPER_H
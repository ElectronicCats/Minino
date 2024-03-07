#ifndef DISPLAY_HELPER_H
#define DISPLAY_HELPER_H

enum MenuLayer {
    LAYER_MAIN_MENU = 0,
};

typedef enum MenuLayer Layer;

char* mainOptions[] = {
    "Applications",
    "Settings",
    "About",
};

#endif // DISPLAY_HELPER_H
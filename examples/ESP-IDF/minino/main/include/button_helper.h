#ifndef BUTTON_HELPER_H
#define BUTTON_HELPER_H

static const char* button_event_table[] = {
    "BUTTON_PRESS_DOWN",
    "BUTTON_PRESS_UP",
    "BUTTON_PRESS_REPEAT",
    "BUTTON_PRESS_REPEAT_DONE",
    "BUTTON_SINGLE_CLICK",
    "BUTTON_DOUBLE_CLICK",
    "BUTTON_MULTIPLE_CLICK",
    "BUTTON_LONG_PRESS_START",
    "BUTTON_LONG_PRESS_HOLD",
    "BUTTON_LONG_PRESS_UP",
};

static const char* button_name_table[] = {
    "BOOT",
    "LEFT",
    "RIGHT",
    "UP",
    "DOWN",
};

enum button_name {
    BOOT = 0,
    LEFT,
    RIGHT,
    UP,
    DOWN,
};

#endif
#ifndef SPAM_SCREENS_H
#define SPAM_SCREENS_H

#include <stdint.h>

void ble_screens_start_scanning_animation(const char* title);
void ble_screens_display_scanning_text(const char* name);
void ble_screens_display_ble_spam(void);

#endif  // SPAM_SCREENS_H


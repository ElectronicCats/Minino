#ifndef SPAM_SCREENS_H
  #define SPAM_SCREENS_H

  #include <stdbool.h>
  #include <stdint.h>

void ble_screens_start_scanning_animation(const char* title);
void ble_screens_display_scanning_text(const char* name);
void ble_screens_display_ble_spam(void);

/**
 * @brief Show that a peer connected to one of the spammed identities.
 *
 * @param name    Model that answered
 * @param success Whether the peer stayed connected past pairing
 */
void ble_screens_display_peer_connected(const char* name, bool success);

#endif  // SPAM_SCREENS_H

void* ble_screens_get_text_mutex(void);

#include "app_manager.h"

void app_manager_enter(const minino_app_t *app) {
  (void)app;
}

void app_manager_exit(void) {
}

bool app_manager_is_active(void) {
  return false;
}

const minino_app_t *app_manager_active(void) {
  return NULL;
}

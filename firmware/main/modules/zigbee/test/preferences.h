#pragma once
static inline int preferences_get_int(const char* k, int d) {
  (void) k;
  return d;
}
static inline void preferences_put_int(const char* k, int v) {
  (void) k;
  (void) v;
}

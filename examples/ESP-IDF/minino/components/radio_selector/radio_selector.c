#include "radio_selector.h"

bool thread_selected;
bool radio_selector_is_thread_enabled() {
  return thread_selected;
}
void radio_selector_enable_thread() {
  thread_selected = true;
}
void radio_selector_disable_thread() {
  thread_selected = false;
}

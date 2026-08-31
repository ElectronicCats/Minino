#include "led_events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "leds.h"
#include "task_manager.h"

static TaskHandle_t led_evenet_task = NULL;
static volatile bool led_event_running = false;

void led_control_ble_tracking(void) {
  led_start_blink(LED_LEFT, 150, 3, 100, 100, 400);
  led_start_blink(LED_RIGHT, 150, 3, 100, 100, 400);
}

void led_control_ble_spam_breathing(void) {
  led_start_breath(LED_LEFT, 100);
  led_start_breath(LED_RIGHT, 100);
}

void led_control_wifi_scanning(void) {
  led_start_breath(LED_LEFT, 100);
  led_start_breath(LED_RIGHT, 100);
}

void led_control_wifi_attacking(void) {
  led_start_blink(LED_LEFT, 255, 5, 50, 50, 100);
  led_start_blink(LED_RIGHT, 255, 3, 100, 100, 200);
}

void led_control_zigbee_scanning(void) {
  led_start_blink(LED_LEFT, 255, 3, 50, 50, 150);
  led_start_blink(LED_RIGHT, 255, 3, 50, 50, 150);
}

void led_control_pulse_leds(void) {
  led_start_blink(LED_LEFT, 255, 1, 150, 10, 0);
  led_start_blink(LED_RIGHT, 255, 1, 150, 10, 0);
}

void led_control_pulse_led_right(void) {
  led_start_blink(LED_RIGHT, 255, 1, 150, 10, 0);
}

void led_control_pulse_led_left(void) {
  led_start_blink(LED_LEFT, 255, 1, 150, 10, 0);
}

void led_control_stop(void) {
  led_event_running = false;
  leds_off();
  if (led_evenet_task != NULL) {
    task_manager_delete(led_evenet_task);
    led_evenet_task = NULL;
  }
}

void led_control_run_effect(effect_control effect_function) {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif
  if (effect_function != NULL) {
    led_event_running = true;
    effect_function();
  }
}

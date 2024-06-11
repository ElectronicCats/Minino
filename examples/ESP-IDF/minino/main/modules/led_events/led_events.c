#include "led_events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "leds.h"

static TaskHandle_t led_evenet_task = NULL;  ////////////

void led_control_ble_tracking(void) {
  led_start_blink(LED_LEFT, 255, 3, 100, 100, 400);
  led_start_blink(LED_RIGHT, 255, 3, 100, 100, 400);
  vTaskSuspend(NULL);  ///////////////////////
}

void led_control_ble_spam_breathing(void) {
  led_start_breath(LED_LEFT, 100);
  led_start_breath(LED_RIGHT, 100);
  vTaskSuspend(NULL);  ///////////////////////
}

void led_control_wifi_scanning(void) {
  led_start_blink(LED_LEFT, 255, 3, 100, 100, 400);
  led_start_blink(LED_RIGHT, 255, 3, 100, 100, 400);
  vTaskSuspend(NULL);  ///////////////////////
}

void led_control_wifi_attacking(void) {
  led_start_blink(LED_LEFT, 255, 5, 50, 50, 100);
  led_start_blink(LED_RIGHT, 255, 3, 100, 100, 200);
  vTaskSuspend(NULL);  ///////////////////////
}

void led_control_zigbee_scanning(void) {
  led_start_blink(LED_LEFT, 255, 3, 100, 100, 400);
  led_start_blink(LED_RIGHT, 255, 3, 100, 100, 400);
  vTaskSuspend(NULL);  ///////////////////////
}

void led_control_stop(void) {
  leds_off();
  vTaskDelete(led_evenet_task);  /////////////
  led_evenet_task = NULL;        ///////////////////
}

void led_control_run_effect(effect_control effect_function) {
  // effect_function();

  xTaskCreate(effect_function, "effect_function", 4096, NULL, 0,
              &led_evenet_task);  ////////////
}

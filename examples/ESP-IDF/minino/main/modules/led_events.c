#include "modules/led_events.h"
#include "freertos/FreeRTOS.h"
#include "rgb_ledc_controller.h"

static TaskHandle_t ble_led_evenet_task = NULL;
static rgb_led_t ble_led_controller;
static bool led_control = false;

void led_control_begin(void) {
  ble_led_controller =
      rgb_led_new(GPIO_LED_RED, GPIO_LED_GREEN, GPIO_LED_BLUE, LEDC_CHANNEL_0,
                  LEDC_CHANNEL_1, LEDC_CHANNEL_2);
  ESP_ERROR_CHECK(rgb_led_init(&ble_led_controller));
  led_control = true;
}

void led_control_game_event_pairing(void) {
  while (led_control) {
    rgb_led_start_breath_effect(&ble_led_controller, BLUE, 100);
    vTaskDelay(pdMS_TO_TICKS(6000));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_game_event_blue_team_turn(void) {
  while (led_control) {
    rgb_led_set_color(&ble_led_controller, BLUE);
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_game_event_red_team_turn(void) {
  while (led_control) {
    rgb_led_set_color(&ble_led_controller, RED);
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_game_event_attacking(void) {
  while (led_control) {
    rgb_led_start_blink_effect(&ble_led_controller, YELLOW, 3, 500, 500, 3000);
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_game_event_red_team_winner(void) {
  while (led_control) {
    rgb_led_start_blink_effect(&ble_led_controller, RED, 3, 300, 300, 1200);
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_game_event_blue_team_winner(void) {
  while (led_control) {
    rgb_led_start_blink_effect(&ble_led_controller, BLUE, 3, 300, 300, 1200);
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_ble_tracking(void) {
  while (led_control) {
    rgb_led_start_blink_effect(&ble_led_controller, BLUE, 3, 100, 100, 400);
    vTaskDelay(pdMS_TO_TICKS(1200));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_ble_spam_breathing(void) {
  while (led_control) {
    rgb_led_start_breath_effect(&ble_led_controller, BLUE, 2000);
    vTaskDelay(pdMS_TO_TICKS(1200));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_wifi_scanning(void) {
  while (led_control) {
    rgb_led_start_blink_effect(&ble_led_controller, GREEN, 3, 100, 100, 400);
    vTaskDelay(pdMS_TO_TICKS(1200));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_wifi_attacking(void) {
  while (led_control) {
    rgb_led_start_blink_effect(&ble_led_controller, RED, 3, 100, 100, 400);
    vTaskDelay(pdMS_TO_TICKS(1200));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_zigbee_scanning(void) {
  while (led_control) {
    rgb_led_start_blink_effect(&ble_led_controller, RED, 3, 100, 200, 400);
    vTaskDelay(pdMS_TO_TICKS(1200));
  }
  // vTaskSuspend(NULL);
  // rgb_led_deinit(&ble_led_controller);
}

void led_control_stop(void) {
  if (led_control) {
    led_control = false;
    rgb_led_stop_any_effect(&ble_led_controller);
  }
}

void led_control_run_effect(effect_coontrol effect_function) {
  if (ble_led_evenet_task != NULL) {
    vTaskDelete(ble_led_evenet_task);
    // rgb_led_deinit(&ble_led_controller);
  }

  if (!led_control) {
    led_control_begin();
  }

  xTaskCreate(effect_function, "effect_function", 8192, NULL, 20,
              &ble_led_evenet_task);
}

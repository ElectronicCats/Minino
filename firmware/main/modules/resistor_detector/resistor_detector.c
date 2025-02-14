#include "resistor_detector.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard_module.h"

#define SAMPLE_COUNT    50
#define SAMPLE_DELAY_MS 5
#define C_DISCHARGE_MS  5
#define TRUE_THRESHOLD  0.8

static const char* TAG = "Resistor_check";

static bool res_detected = false;

bool resistor_detector(gpio_num_t gpio_num) {
  gpio_config_t temp_config = {.pin_bit_mask = (1ULL << gpio_num),
                               .mode = GPIO_MODE_OUTPUT,
                               .pull_up_en = 0,
                               .pull_down_en = 0,
                               .intr_type = 0};
  esp_err_t ret = gpio_config(&temp_config);
  gpio_set_level(gpio_num, 0);
  vTaskDelay(pdMS_TO_TICKS(C_DISCHARGE_MS));

  gpio_config_t new_config = {.pin_bit_mask = (1ULL << gpio_num),
                              .mode = GPIO_MODE_INPUT,
                              .pull_up_en = 0,
                              .pull_down_en = 0,
                              .intr_type = 0};
  ret = gpio_config(&new_config);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error configurando GPIO %d", gpio_num);
    return false;
  }

  int high_count = 0;
  int low_count = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    int pin_value = gpio_get_level(gpio_num);

    if (pin_value == 1) {
      high_count++;
    } else {
      low_count++;
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }

  // printf("UP: %d | DOWN: %d\n", high_count, low_count);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error restaurando configuración GPIO %d", gpio_num);
    res_detected = false;
    return false;
  }
  if (high_count > SAMPLE_COUNT * TRUE_THRESHOLD) {
    // printf("RES DETECTED\n");
    res_detected = true;
    return true;
  } else {
    // printf("RES NO DETECTED\n");
    res_detected = false;
    return false;
  }
}

bool resistor_detector_get_result() {
  return res_detected;
}
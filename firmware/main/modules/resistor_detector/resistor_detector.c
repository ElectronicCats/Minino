#include "resistor_detector.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard_module.h"

#define SAMPLE_COUNT    50
#define SAMPLE_DELAY_MS 5
#define C_DISCHARGE_MS  5

static const char* TAG = "Resistor_check";

esp_err_t resistor_detector(gpio_num_t gpio_num) {
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
    return ret;
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

  printf("UP: %d | DOWN: %d\n", high_count, low_count);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error restaurando configuración GPIO %d", gpio_num);
    return ret;
  }

  return ESP_OK;
}
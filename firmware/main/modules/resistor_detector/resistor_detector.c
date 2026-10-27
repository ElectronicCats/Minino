#include "resistor_detector.h"

#ifdef ESP_PLATFORM
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "keyboard_module.h"

  #define SAMPLE_COUNT    50
  #define SAMPLE_DELAY_MS 5
  #define C_DISCHARGE_MS  5
  #define TRUE_THRESHOLD  0.8

static const char* TAG = "Resistor_check";
#endif

static bool res_detected = false;

resistor_calc_status_t resistor_calculate_pullup_adc(uint32_t adc_raw,
                                                     uint32_t adc_max,
                                                     float r_known,
                                                     float* r_out) {
  if (r_out == NULL || r_known <= 0.0f) {
    return RESISTOR_CALC_ERROR_INVALID_ARG;
  }
  if (adc_max == 0) {
    return RESISTOR_CALC_ERROR_DIV_ZERO;
  }
  if (adc_raw > adc_max) {
    return RESISTOR_CALC_ERROR_OUT_OF_RANGE;
  }
  if (adc_raw == 0) {
    /* 0V across known resistor in pull-up divider: Rx is infinite (open
     * circuit). Prevents division by zero: Rx = R_known * (adc_max - adc_raw) /
     * adc_raw */
    *r_out = -1.0f;
    return RESISTOR_CALC_OPEN_CIRCUIT;
  }
  if (adc_raw == adc_max) {
    /* Full ADC reading: pin pulled hard to Vcc, Rx = 0 (short circuit to Vcc).
     */
    *r_out = 0.0f;
    return RESISTOR_CALC_SHORT_CIRCUIT;
  }

  *r_out = r_known * ((float) (adc_max - adc_raw) / (float) adc_raw);
  return RESISTOR_CALC_OK;
}

resistor_calc_status_t resistor_calculate_pulldown_adc(uint32_t adc_raw,
                                                       uint32_t adc_max,
                                                       float r_known,
                                                       float* r_out) {
  if (r_out == NULL || r_known <= 0.0f) {
    return RESISTOR_CALC_ERROR_INVALID_ARG;
  }
  if (adc_max == 0) {
    return RESISTOR_CALC_ERROR_DIV_ZERO;
  }
  if (adc_raw > adc_max) {
    return RESISTOR_CALC_ERROR_OUT_OF_RANGE;
  }
  if (adc_raw == adc_max) {
    /* Full ADC reading in pull-down divider: Rx is open circuit or short to
     * Vcc. Prevents division by zero: Rx = R_known * adc_raw / (adc_max -
     * adc_raw) */
    *r_out = -1.0f;
    return RESISTOR_CALC_OPEN_CIRCUIT;
  }
  if (adc_raw == 0) {
    /* 0V across pull-down: Rx = 0 (short circuit to GND). */
    *r_out = 0.0f;
    return RESISTOR_CALC_SHORT_CIRCUIT;
  }

  *r_out = r_known * ((float) adc_raw / (float) (adc_max - adc_raw));
  return RESISTOR_CALC_OK;
}

resistor_calc_status_t resistor_calculate_voltage_divider(float v_measured,
                                                          float v_cc,
                                                          float r_known,
                                                          bool is_pullup,
                                                          float* r_out) {
  if (r_out == NULL || v_cc <= 0.0f || r_known <= 0.0f || v_measured < 0.0f) {
    return RESISTOR_CALC_ERROR_INVALID_ARG;
  }
  if (v_measured > v_cc) {
    return RESISTOR_CALC_ERROR_OUT_OF_RANGE;
  }

  if (is_pullup) {
    if (v_measured == 0.0f) {
      *r_out = -1.0f;
      return RESISTOR_CALC_OPEN_CIRCUIT;
    }
    if (v_measured >= v_cc) {
      *r_out = 0.0f;
      return RESISTOR_CALC_SHORT_CIRCUIT;
    }
    *r_out = r_known * ((v_cc - v_measured) / v_measured);
    return RESISTOR_CALC_OK;
  } else {
    if (v_measured >= v_cc) {
      *r_out = -1.0f;
      return RESISTOR_CALC_OPEN_CIRCUIT;
    }
    if (v_measured == 0.0f) {
      *r_out = 0.0f;
      return RESISTOR_CALC_SHORT_CIRCUIT;
    }
    *r_out = r_known * (v_measured / (v_cc - v_measured));
    return RESISTOR_CALC_OK;
  }
}

bool resistor_detector_evaluate_samples(int high_count,
                                        int sample_count,
                                        float threshold,
                                        bool* detected) {
  if (detected == NULL) {
    return false;
  }
  if (sample_count <= 0 || threshold < 0.0f || threshold > 1.0f ||
      high_count < 0 || high_count > sample_count) {
    *detected = false;
    return false;
  }

  *detected = ((float) high_count / (float) sample_count) > threshold;
  return true;
}

#ifdef ESP_PLATFORM
bool resistor_detector(gpio_num_t gpio_num) {
  /* Step 1: Discharge parasitic capacitance on the pin */
  gpio_config_t temp_config = {.pin_bit_mask = (1ULL << gpio_num),
                               .mode = GPIO_MODE_OUTPUT,
                               .pull_up_en = 0,
                               .pull_down_en = 0,
                               .intr_type = 0};
  esp_err_t ret = gpio_config(&temp_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error configuring GPIO %d as output", gpio_num);
    res_detected = false;
    return false;
  }
  gpio_set_level(gpio_num, 0);
  vTaskDelay(pdMS_TO_TICKS(C_DISCHARGE_MS));

  /* Step 2: Switch to high-impedance input to observe pull-up */
  gpio_config_t new_config = {.pin_bit_mask = (1ULL << gpio_num),
                              .mode = GPIO_MODE_INPUT,
                              .pull_up_en = 0,
                              .pull_down_en = 0,
                              .intr_type = 0};
  ret = gpio_config(&new_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error configuring GPIO %d as input", gpio_num);
    res_detected = false;
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
    vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
  }

  bool detected = false;
  if (!resistor_detector_evaluate_samples(high_count, SAMPLE_COUNT,
                                          TRUE_THRESHOLD, &detected)) {
    res_detected = false;
    return false;
  }

  res_detected = detected;
  return res_detected;
}
#endif

bool resistor_detector_get_result(void) {
  return res_detected;
}
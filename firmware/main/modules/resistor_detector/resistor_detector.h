#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef ESP_PLATFORM
  #include "driver/gpio.h"
  #include "esp_log.h"
  #include "esp_system.h"

bool resistor_detector(gpio_num_t gpio_num);
#endif

bool resistor_detector_get_result(void);

typedef enum {
  RESISTOR_CALC_OK = 0,
  RESISTOR_CALC_ERROR_INVALID_ARG,
  RESISTOR_CALC_ERROR_DIV_ZERO,
  RESISTOR_CALC_ERROR_OUT_OF_RANGE,
  RESISTOR_CALC_OPEN_CIRCUIT,
  RESISTOR_CALC_SHORT_CIRCUIT,
} resistor_calc_status_t;

resistor_calc_status_t resistor_calculate_pullup_adc(uint32_t adc_raw,
                                                     uint32_t adc_max,
                                                     float r_known,
                                                     float* r_out);

resistor_calc_status_t resistor_calculate_pulldown_adc(uint32_t adc_raw,
                                                       uint32_t adc_max,
                                                       float r_known,
                                                       float* r_out);

resistor_calc_status_t resistor_calculate_voltage_divider(float v_measured,
                                                          float v_cc,
                                                          float r_known,
                                                          bool is_pullup,
                                                          float* r_out);

bool resistor_detector_evaluate_samples(int high_count,
                                        int sample_count,
                                        float threshold,
                                        bool* detected);
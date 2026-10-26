/* Host unit tests for resistor detector and calculations.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "resistor_detector.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * 1. Pull-up Resistor Calculation Tests (ADC reading 0 to MAX)
 * ========================================================================= */

void test_pullup_adc_normal_calculation(void) {
  float r_out = 0.0f;
  const uint32_t adc_max = 4095;
  const float r_known = 10000.0f; /* 10k reference to GND */

  /* If ADC reading is mid-scale (half Vcc), Rx == R_known */
  resistor_calc_status_t status =
      resistor_calculate_pullup_adc(2048, adc_max, r_known, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_OK, status);
  /* 10000 * (4095 - 2048) / 2048 = ~9995.1 Ohm */
  TEST_ASSERT_FLOAT_WITHIN(20.0f, 10000.0f, r_out);
}

void test_pullup_adc_zero_reading_prevents_div_by_zero(void) {
  float r_out = 0.0f;
  const uint32_t adc_max = 4095;
  const float r_known = 10000.0f;

  /* When ADC reading is 0, V_adc = 0V -> R_x is open circuit.
   * Must return RESISTOR_CALC_OPEN_CIRCUIT and NOT divide by zero! */
  resistor_calc_status_t status =
      resistor_calculate_pullup_adc(0, adc_max, r_known, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_OPEN_CIRCUIT, status);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r_out);
}

void test_pullup_adc_max_reading_short_circuit(void) {
  float r_out = -1.0f;
  const uint32_t adc_max = 4095;
  const float r_known = 10000.0f;

  /* When ADC reading is adc_max, V_adc = Vcc -> R_x is shorted to Vcc (0 Ohm).
   */
  resistor_calc_status_t status =
      resistor_calculate_pullup_adc(adc_max, adc_max, r_known, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_SHORT_CIRCUIT, status);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, r_out);
}

void test_pullup_adc_invalid_arguments(void) {
  float r_out = 0.0f;

  /* Null pointer */
  TEST_ASSERT_EQUAL_INT(
      RESISTOR_CALC_ERROR_INVALID_ARG,
      resistor_calculate_pullup_adc(100, 4095, 10000.0f, NULL));

  /* Non-positive reference resistor */
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_ERROR_INVALID_ARG,
                        resistor_calculate_pullup_adc(100, 4095, 0.0f, &r_out));
  TEST_ASSERT_EQUAL_INT(
      RESISTOR_CALC_ERROR_INVALID_ARG,
      resistor_calculate_pullup_adc(100, 4095, -100.0f, &r_out));

  /* ADC max is 0 (division by zero guard) */
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_ERROR_DIV_ZERO,
                        resistor_calculate_pullup_adc(0, 0, 10000.0f, &r_out));

  /* ADC raw > ADC max */
  TEST_ASSERT_EQUAL_INT(
      RESISTOR_CALC_ERROR_OUT_OF_RANGE,
      resistor_calculate_pullup_adc(5000, 4095, 10000.0f, &r_out));
}

/* =========================================================================
 * 2. Pull-down Resistor Calculation Tests (ADC reading 0 to MAX)
 * ========================================================================= */

void test_pulldown_adc_normal_calculation(void) {
  float r_out = 0.0f;
  const uint32_t adc_max = 4095;
  const float r_known = 10000.0f; /* 10k reference to Vcc */

  /* If ADC reading is mid-scale, Rx == R_known */
  resistor_calc_status_t status =
      resistor_calculate_pulldown_adc(2048, adc_max, r_known, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_OK, status);
  TEST_ASSERT_FLOAT_WITHIN(20.0f, 10000.0f, r_out);
}

void test_pulldown_adc_max_reading_prevents_div_by_zero(void) {
  float r_out = 0.0f;
  const uint32_t adc_max = 4095;
  const float r_known = 10000.0f;

  /* In pull-down, denominator is (adc_max - adc_raw).
   * When adc_raw == adc_max, denominator is 0.
   * Must return RESISTOR_CALC_OPEN_CIRCUIT and NOT divide by zero! */
  resistor_calc_status_t status =
      resistor_calculate_pulldown_adc(adc_max, adc_max, r_known, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_OPEN_CIRCUIT, status);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r_out);
}

void test_pulldown_adc_zero_reading_short_circuit(void) {
  float r_out = -1.0f;
  const uint32_t adc_max = 4095;
  const float r_known = 10000.0f;

  /* When adc_raw is 0, V_adc = 0V -> Rx is shorted to GND (0 Ohm). */
  resistor_calc_status_t status =
      resistor_calculate_pulldown_adc(0, adc_max, r_known, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_SHORT_CIRCUIT, status);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, r_out);
}

void test_pulldown_adc_invalid_arguments(void) {
  float r_out = 0.0f;

  TEST_ASSERT_EQUAL_INT(
      RESISTOR_CALC_ERROR_INVALID_ARG,
      resistor_calculate_pulldown_adc(100, 4095, 10000.0f, NULL));
  TEST_ASSERT_EQUAL_INT(
      RESISTOR_CALC_ERROR_INVALID_ARG,
      resistor_calculate_pulldown_adc(100, 4095, -50.0f, &r_out));
  TEST_ASSERT_EQUAL_INT(
      RESISTOR_CALC_ERROR_DIV_ZERO,
      resistor_calculate_pulldown_adc(0, 0, 10000.0f, &r_out));
  TEST_ASSERT_EQUAL_INT(
      RESISTOR_CALC_ERROR_OUT_OF_RANGE,
      resistor_calculate_pulldown_adc(4096, 4095, 10000.0f, &r_out));
}

/* =========================================================================
 * 3. Analog Voltage Divider Calculation Tests
 * ========================================================================= */

void test_voltage_divider_calculations(void) {
  float r_out = 0.0f;
  const float v_cc = 3.3f;
  const float r_known = 10000.0f;

  /* Normal pullup: V_adc = 1.65V -> Rx = 10000 Ohm */
  resistor_calc_status_t status =
      resistor_calculate_voltage_divider(1.65f, v_cc, r_known, true, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_OK, status);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 10000.0f, r_out);

  /* Pullup with 0V -> open circuit, no div by zero */
  status =
      resistor_calculate_voltage_divider(0.0f, v_cc, r_known, true, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_OPEN_CIRCUIT, status);

  /* Pullup with Vcc -> short circuit to Vcc */
  status =
      resistor_calculate_voltage_divider(v_cc, v_cc, r_known, true, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_SHORT_CIRCUIT, status);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, r_out);

  /* Pulldown with Vcc -> open circuit, no div by zero */
  status =
      resistor_calculate_voltage_divider(v_cc, v_cc, r_known, false, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_OPEN_CIRCUIT, status);

  /* Pulldown with 0V -> short circuit to GND */
  status =
      resistor_calculate_voltage_divider(0.0f, v_cc, r_known, false, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_SHORT_CIRCUIT, status);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, r_out);

  /* Voltage exceeding Vcc */
  status =
      resistor_calculate_voltage_divider(3.5f, v_cc, r_known, true, &r_out);
  TEST_ASSERT_EQUAL_INT(RESISTOR_CALC_ERROR_OUT_OF_RANGE, status);
}

/* =========================================================================
 * 4. Sample Evaluation & Division by Zero Guard Tests
 * ========================================================================= */

void test_sample_evaluation_prevents_zero_sample_count(void) {
  bool detected = false;

  /* sample_count = 0 must NOT cause division by zero! */
  bool ok = resistor_detector_evaluate_samples(0, 0, 0.8f, &detected);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_FALSE(detected);

  /* negative sample count */
  ok = resistor_detector_evaluate_samples(5, -1, 0.8f, &detected);
  TEST_ASSERT_FALSE(ok);
}

void test_sample_evaluation_threshold_decision(void) {
  bool detected = false;

  /* 50 samples, threshold 0.8 -> 40 samples is threshold.
   * If high_count > 40, detected = true. */
  bool ok = resistor_detector_evaluate_samples(41, 50, 0.8f, &detected);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(detected);

  /* 40 / 50 = 0.80 -> not strictly greater than 0.8 */
  ok = resistor_detector_evaluate_samples(40, 50, 0.8f, &detected);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_FALSE(detected);

  /* 10 / 50 = 0.20 -> false */
  ok = resistor_detector_evaluate_samples(10, 50, 0.8f, &detected);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_FALSE(detected);
}

void test_sample_evaluation_out_of_range(void) {
  bool detected = false;

  /* high_count > sample_count */
  TEST_ASSERT_FALSE(
      resistor_detector_evaluate_samples(51, 50, 0.8f, &detected));

  /* negative high_count */
  TEST_ASSERT_FALSE(
      resistor_detector_evaluate_samples(-1, 50, 0.8f, &detected));

  /* threshold out of range */
  TEST_ASSERT_FALSE(
      resistor_detector_evaluate_samples(25, 50, 1.5f, &detected));
  TEST_ASSERT_FALSE(
      resistor_detector_evaluate_samples(25, 50, -0.1f, &detected));

  /* NULL detected pointer */
  TEST_ASSERT_FALSE(resistor_detector_evaluate_samples(25, 50, 0.8f, NULL));
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_pullup_adc_normal_calculation);
  RUN_TEST(test_pullup_adc_zero_reading_prevents_div_by_zero);
  RUN_TEST(test_pullup_adc_max_reading_short_circuit);
  RUN_TEST(test_pullup_adc_invalid_arguments);

  RUN_TEST(test_pulldown_adc_normal_calculation);
  RUN_TEST(test_pulldown_adc_max_reading_prevents_div_by_zero);
  RUN_TEST(test_pulldown_adc_zero_reading_short_circuit);
  RUN_TEST(test_pulldown_adc_invalid_arguments);

  RUN_TEST(test_voltage_divider_calculations);

  RUN_TEST(test_sample_evaluation_prevents_zero_sample_count);
  RUN_TEST(test_sample_evaluation_threshold_decision);
  RUN_TEST(test_sample_evaluation_out_of_range);

  return UNITY_END();
}

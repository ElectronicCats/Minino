#pragma once

/**
 * @brief Initialize the buzzer
 *
 * @param pin The pin of the buzzer
 *
 * @return void
 */
void buzzer_begin(uint8_t pin);

/**
 * @brief Set the frequency of the buzzer
 *
 * @param freq The frequency of the buzzer
 * @return void
 */
void buzzer_set_freq(uint32_t freq);

/**
 * @brief Set the duty cycle of the buzzer
 *
 * @param duty The duty cycle of the buzzer
 *
 * @return void
 */
void buzzer_set_duty(uint32_t duty);

/**
 * @brief Play the buzzer
 *
 * @param volume The volume of the buzzer
 * @return void
 */
void buzzer_play();

/**
 * @brief Play the buzzer for a specific duration.
 * Does not require `buzzer_stop`
 *
 * @param duration The duration of the buzzer
 *
 * @return void
 */
void buzzer_play_for(uint32_t duration);

/**
 * @brief Stop the buzzer
 *
 * @return void
 */
void buzzer_stop();

void buzzer_enable();
void buzzer_disable();
#ifndef BUZZER_H
#define BUZZER_H

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
void buzzer_set_freq(uint8_t freq);

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
 * @brief Stop the buzzer
 *
 * @return void
 */
void buzzer_stop();

#endif  // BUZZER_H

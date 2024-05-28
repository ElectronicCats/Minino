#ifndef LEDC_CONTROLLER_H
#define LEDC_CONTROLLER_H

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure to hold LED configuration
 */
typedef struct {
  gpio_num_t pin;          // GPIO pin for the LED
  ledc_channel_t channel;  // LEDC channel for the LED
} led_t;

/**
 * @brief Deinitializes the LED module and releases allocated resources.
 *
 * This function deinitializes the LED module for the specified LED. It
 * stops and deletes any active effects associated with the LED and removes it
 * from the module's internal data structure.
 *
 * @param led_cfg Pointer to the LED configuration structure.
 * @return
 *     - ESP_OK if the LED was successfully deinitialized.
 *     - ESP_ERR_INVALID_ARG if the input argument is invalid.
 *     - ESP_FAIL if the LED is not found in the module's internal data
 * structure.
 */
esp_err_t led_controller_led_deinit(led_t* led_cfg);

/**
 * @brief Initialize the LEDC to control a LED.
 *
 * @description This function initializes the ESP32's LEDC (LED Controller)
 * hardware peripheral to control a LED. The function first checks if the
 * provided GPIO number is valid for output, and if the LEDC channel is not
 * already in use. It then configures the GPIO for output and sets up the LEDC
 * peripheral to control the LED. The function returns ESP_OK if the
 * initialization was successful. If there was an error, it returns an
 * appropriate ESP_ERR code.
 *
 * @param config Pointer to a `led_t` struct containing the
 * configuration data for the LED.
 * @return esp_err_t ESP_OK if successful, or an error code if not.
 */
esp_err_t led_controller_led_init(led_t* led_cfg);

/**
 * @brief Create and initialize a new LED instance.
 *
 * @param pin GPIO pin for the LED.
 * @param channel LEDC channel for the LED.
 *
 * @return The initialized LED instance.
 */
led_t led_controller_led_new(gpio_num_t pin, ledc_channel_t ch);

/**
 * @brief Starts the breath effect for the specified LED.
 *
 * @param led Pointer to the LED instance.
 * @param duration_ms Duration of the breath effect in milliseconds.
 * @return esp_err_t ESP_OK if successful, or an error code if the breath effect
 * cannot be started.
 */
esp_err_t led_controller_start_breath_effect(led_t* led, uint16_t duration_ms);

/**
 * @brief Start a blink effect for the specified LED.
 *
 * This function starts a blink effect for the specified LED with the given
 * parameters. The LED will alternate between the specified intensity and off
 * state for the specified number of pulses. The `time_on` parameter specifies
 * the duration in milliseconds for which the LED will be on, while the
 * `time_off` parameter specifies the duration in milliseconds for which the LED
 * will be off. The `time_out` parameter specifies an optional timeout period in
 * milliseconds after which the blink effect will stop automatically.
 *
 * @param led Pointer to the LED structure.
 * @param duty The duty cycle.
 * @param pulse_count The number of pulses for the blink effect.
 * @param time_on The duration in milliseconds for which the LED will be on
 * during each pulse.
 * @param time_off The duration in milliseconds for which the LED will be off
 * during each pulse.
 * @param time_out The optional timeout period in milliseconds after which the
 * blink effect will stop automatically. Set to 0 to disable timeout.
 * @return
 *     - ESP_OK if the blink effect was successfully started.
 *     - ESP_ERR_INVALID_ARG if the input argument is invalid.
 *     - ESP_FAIL if the LED is not found in the LED effects array or the
 * maximum number of LED effects is reached.
 *     - Other error codes if there was an error starting the timer.
 */
esp_err_t led_controller_start_blink_effect(led_t* led,
                                            uint8_t duty,
                                            uint8_t pulse_count,
                                            uint32_t time_on,
                                            uint32_t time_off,
                                            uint32_t time_out);

/**
 * @brief Stops the active effect for a specific LED.
 *
 * @param led Pointer to the LED structure.
 * @return ESP_OK if the effect was successfully stopped, ESP_FAIL otherwise.
 */
esp_err_t led_controller_stop_any_effect(led_t* led);

/**
 * @brief Turns on the specified LED.
 *
 * @param led Pointer to the LED structure.
 */
void led_controller_led_on(led_t* led);

/**
 * @brief Turns off the specified LED.
 * @param led Pointer to the LED structure.
 */
void led_controller_led_off(led_t* led);

/**
 * @brief Sets the intensity of a LED
 *
 * @param led Pointer to the LED structure.
 * @param duty The 8-bit duty cycle value.
 */
void led_controller_set_duty(led_t* led, uint8_t duty);

#ifdef __cplusplus
}
#endif

#endif  // LEDC_CONTROLLER_H

#ifndef RGB_LEDC_CONTROLLER_H
#define RGB_LEDC_CONTROLLER_H

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure to hold RGB LED configuration
 */
typedef struct {
  gpio_num_t red_pin;            // GPIO pin for the red LED
  gpio_num_t green_pin;          // GPIO pin for the green LED
  gpio_num_t blue_pin;           // GPIO pin for the blue LED
  ledc_channel_t red_channel;    // LEDC channel for the red LED
  ledc_channel_t green_channel;  // LEDC channel for the green LED
  ledc_channel_t blue_channel;   // LEDC channel for the blue LED
} rgb_led_t;

/**
 * @brief Deinitializes the RGB LED module and releases allocated resources.
 *
 * This function deinitializes the RGB LED module for the specified LED. It
 * stops and deletes any active effects associated with the LED and removes it
 * from the module's internal data structure.
 *
 * @param rgb_led_cfg Pointer to the RGB LED configuration structure.
 * @return
 *     - ESP_OK if the LED was successfully deinitialized.
 *     - ESP_ERR_INVALID_ARG if the input argument is invalid.
 *     - ESP_FAIL if the LED is not found in the module's internal data
 * structure.
 */
esp_err_t rgb_led_deinit(rgb_led_t* rgb_led_cfg);

/**
 * @brief Initialize the LEDC to control an RGB LED.
 *
 * @description This function initializes the ESP32's LEDC (LED Controller)
 * hardware peripheral to control an RGB LED. The RGB LED is represented by an
 * `rgb_led_t` struct which contains the GPIO numbers and LEDC channels for the
 * red, green, and blue LEDs. The function first checks if the provided GPIO
 * numbers are valid for output, and if the LEDC channels are not already in
 * use. It then configures the GPIOs for output and sets up the LEDC peripheral
 * to control the LEDs. The function returns ESP_OK if the initialization was
 * successful. If there was an error, it returns an appropriate ESP_ERR code.
 *
 * @param rgb_led_cfg Pointer to an `rgb_led_t` struct containing the
 * configuration data for the RGB LED.
 * @return esp_err_t ESP_OK if successful, or an error code if not.
 */
esp_err_t rgb_led_init(rgb_led_t* rgb_led_cfg);

/**
 * @brief Create and initialize a new RGB LED instance.
 *
 * @param red_pin GPIO pin for the red LED.
 * @param green_pin GPIO pin for the green LED.
 * @param blue_pin GPIO pin for the blue LED.
 * @param red_channel LEDC channel for the red LED.
 * @param green_channel LEDC channel for the green LED.
 * @param blue_channel LEDC channel for the blue LED.
 *
 * @return The initialized RGB LED instance.
 */
rgb_led_t rgb_led_new(gpio_num_t red_pin,
                      gpio_num_t green_pin,
                      gpio_num_t blue_pin,
                      ledc_channel_t red_ch,
                      ledc_channel_t green_ch,
                      ledc_channel_t blue_ch);

/**
 * @brief Set the color of an RGB LED.
 *
 * @description This function sets the color of an RGB LED by setting the duty
 * cycle of the LEDC channels controlling the red, green, and blue LEDs. The
 * color is specified as a 24-bit RGB color value. The red, green, and blue
 * components are extracted from the color value, and the LEDC duty cycle is set
 * proportional to the component value. Since the LEDC timer resolution is 8
 * bits, the color component values are used directly as the duty cycle. If an
 * error occurs while setting the duty cycle, an appropriate ESP_ERR code is
 * returned.
 *
 * @param rgb_led A pointer to the rgb_led_t structure that contains the
 * configuration of the RGB LED.
 * @param color The 24-bit RGB color value. The red component is in the most
 * significant 8 bits, followed by green, and then blue.
 * @return esp_err_t ESP_OK if successful, or an error code if not.
 */
esp_err_t rgb_led_set_color(rgb_led_t* rgb_led, uint32_t color);

/**
 * @brief Set the color of an RGB LED with a smooth transition effect.
 *
 * This function sets the color of an RGB LED and smoothly transitions it to the
 * new color over the specified transition duration. The function takes the RGB
 * LED configuration, the current color, the new color, and the duration of the
 * transition in milliseconds. It sets the LED to the new color and creates a
 * timer to gradually transition it back to the previous color after the
 * specified duration. The previous color is stored and used in the transition
 * callback function.
 *
 * @param rgb_led Pointer to the rgb_led_t structure that contains the
 * configuration of the RGB LED.
 * @param current_color The current color of the LED in RGB format (e.g.,
 * 0xRRGGBB).
 * @param new_color The new color to transition the LED to in RGB format.
 * @param effect_duration The duration of the color transition in milliseconds.
 * @param callback Function pointer to the callback triggered after transition
 * completion.
 *
 * @return
 *     - ESP_OK if successful
 *     - ESP_FAIL if a transition is already in progress
 *     - Other error codes if an error occurred
 */
esp_err_t rgb_led_start_transition_effect(rgb_led_t* rgb_led,
                                          uint32_t current_color,
                                          uint32_t new_color,
                                          uint32_t effect_duration,
                                          void (*callback)(rgb_led_t*));

/**
 * @brief Start the breath effect for the specified LED.
 *
 * @param led Pointer to the RGB LED instance.
 * @param color Color for the breath effect.
 * @param duration_ms Duration of the breath effect in milliseconds.
 * @return esp_err_t ESP_OK if successful, or an error code if the breath effect
 * cannot be started.
 */
esp_err_t rgb_led_start_breath_effect(rgb_led_t* rgb_led,
                                      uint32_t color,
                                      uint16_t duration_ms);

/**
 * @brief Start a blink effect for the specified LED.
 *
 * This function starts a blink effect for the specified LED with the given
 * parameters. The LED will alternate between the specified color and off state
 * for the specified number of pulses. The `time_on` parameter specifies the
 * duration in milliseconds for which the LED will be on, while the `time_off`
 * parameter specifies the duration in milliseconds for which the LED will be
 * off. The `time_out` parameter specifies an optional timeout period in
 * milliseconds after which the blink effect will stop automatically.
 *
 * @param led Pointer to the LED structure.
 * @param color The color to blink (RGB value).
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
esp_err_t rgb_led_start_blink_effect(rgb_led_t* rgb_led,
                                     uint32_t color,
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
esp_err_t rgb_led_stop_any_effect(rgb_led_t* rgb_led);

#ifdef __cplusplus
}
#endif

#endif  // RGB_LEDC_CONTROLLER_H

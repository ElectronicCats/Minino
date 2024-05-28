#include "rgb_ledc_controller.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "string.h"

#define MIN(a, b) \
  (((a) < (b))    \
       ? (a)      \
       : (b))  // Macro function to get the minimum value between two numbers.

static const char* TAG = "LED_RGB";

// RGB LED Effect State Enum
// This enum represents the different states of the RGB LED effects.
typedef enum {
  RGB_LED_EFFECT_NONE,        // No effect active
  RGB_LED_EFFECT_TRANSITION,  // Transition effect active
  RGB_LED_EFFECT_BREATH,      // Breath effect active
  RGB_LED_EFFECT_BLINK        // Blink effect active
} rgb_led_effect_state_t;

// RGB LED Transition Structure
// This structure represents the transition effect data for an RGB LED.
typedef struct {
  rgb_led_t* led;                // Pointer to the RGB LED
  esp_timer_handle_t timer;      // Timer handle for the transition effect
  uint32_t color;                // Color of the transition effect
  void (*callback)(rgb_led_t*);  // Callback function
} rgb_led_transition_t;

// RGB LED Breath Structure
// This structure represents the breath effect data for an RGB LED.
typedef struct {
  rgb_led_t* led;            // Pointer to the RGB LED
  esp_timer_handle_t timer;  // Timer handle for the breath effect
  uint32_t color;            // Color of the breath effect
  float brightness;          // Current brightness of the LED
  float step;                // Step size for increasing/decreasing brightness
} rgb_led_breath_t;

// RGB LED Blink Structure
// This structure represents the blink effect data for an RGB LED.
typedef struct {
  rgb_led_t* led;            // Pointer to the RGB LED
  esp_timer_handle_t timer;  // Timer handle for the blink effect
  uint32_t color;            // Color of the blink effect
  uint32_t time_on;          // Duration of the LED ON state during each pulse
  uint32_t time_off;         // Duration of the LED OFF state during each pulse
  uint32_t time_out;         // Timeout duration for the blink effect
  uint8_t pulse_count;       // Number of pulses for the blink effect
} rgb_led_blink_t;

// RGB LED Effects Structure
// This structure represents the effects data for an RGB LED.
typedef struct {
  rgb_led_t* led;                          // Pointer to the RGB LED
  rgb_led_effect_state_t effect_state;     // Current effect state
  rgb_led_transition_t transition_effect;  // Transition effect data
  rgb_led_breath_t breath_effect;          // Breath effect data
  rgb_led_blink_t blink_effect;            // Blink effect data
  // Add any additional fields you need for other effects
} rgb_led_effects_t;

#define COLOR_BLACK 0x000000

// Maximum number of LEDs for breath effect
#define MAX_RGB_LEDS 2  // max 8 ledc_channels -> 3 x 2 = 6pin

// Array to hold the breath and blink effect data for multiple LEDs
static rgb_led_effects_t led_effects[MAX_RGB_LEDS];

// Number of breath and blink effect LEDs currently active
static int num_led_effects = 0;

static int get_led_index(rgb_led_t* led) {
  // Check if the blink effect is already active for the specified LED
  for (int i = 0; i < num_led_effects; i++) {
    if (led_effects[i].led == led) {
      return i;
    }
  }
  return -1;
}

/**
 * @brief Sets up a LEDC channel to control a specific LED.
 *
 * @description This function sets up one of the ESP32's LEDC (LED Controller)
 * channels to control a specific LED. It first sets up a LEDC timer with a
 * predefined configuration (8-bit duty resolution, 5000 Hz frequency,
 * high-speed mode, timer number 0). It then configures the LEDC channel to use
 * the provided GPIO and the LEDC timer. The LED is initially off (duty = 0).
 * The function returns ESP_OK if the setup was successful. If there was an
 * error, it returns an appropriate ESP_ERR code.
 *
 * @param led_gpio The GPIO number of the LED to control.
 * @param led_channel The LEDC channel to use for the LED.
 * @return esp_err_t ESP_OK if successful, or an error code if not.
 */
static esp_err_t setup_ledc(int led_gpio, ledc_channel_t led_channel) {
  // Set LEDC timer configuration
  ledc_timer_config_t ledc_timer = {
      .duty_resolution = LEDC_TIMER_8_BIT,  // Resolution of PWM duty
      .freq_hz = 5000,                      // Frequency of PWM signal
      .speed_mode = LEDC_LOW_SPEED_MODE,    // Timer mode
      .timer_num = LEDC_TIMER_0             // Timer index
  };

  // Set the configuration
  esp_err_t err = ledc_timer_config(&ledc_timer);
  if (err != ESP_OK)
    return err;

  // Prepare individual configuration for each LED
  ledc_channel_config_t ledc_channel_cfg = {
      .channel = led_channel,
      .duty = 0,  // Initial duty value (LED is off)
      .gpio_num = led_gpio,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .hpoint = 0,
      .timer_sel = LEDC_TIMER_0};

  ledc_fade_func_install(NULL);

  // Set the configuration
  return ledc_channel_config(&ledc_channel_cfg);
}

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
static esp_err_t set_color(rgb_led_t* rgb_led, uint32_t color) {
  if (!rgb_led) {
    return ESP_ERR_INVALID_ARG;
  }
  // Extract the red, green, and blue components from the color value
  uint32_t red = color >> 16;
  uint32_t green = (color >> 8) & 0xFF;
  uint32_t blue = color & 0xFF;

  // Set the duty cycle for each LED. The duty cycle is proportional to the
  // color component value. Since the LEDC timer resolution is 8 bits, we can
  // use the color component value directly.
  ESP_RETURN_ON_ERROR(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE,
                                               rgb_led->red_channel, red, 0),
                      TAG, "Failed to set red duty");
  ESP_RETURN_ON_ERROR(
      ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, rgb_led->green_channel,
                               green, 0),
      TAG, "Failed to set green duty");
  ESP_RETURN_ON_ERROR(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE,
                                               rgb_led->blue_channel, blue, 0),
                      TAG, "Failed to set blue duty");

  return ESP_OK;
}

/**
 * @brief Initialize the LEDC to control an RGB LED.
 *
 * @description This function initializes the ESP32's LEDC (LED Controller)
 * hardware peripheral to control an RGB LED. The RGB LED is represented by an
 * `rgb_led_cfg_t` struct which contains the GPIO numbers and LEDC channels for
 * the red, green, and blue LEDs. The function first checks if the provided GPIO
 * numbers are valid for output, and if the LEDC channels are not already in
 * use. It then configures the GPIOs for output and sets up the LEDC peripheral
 * to control the LEDs. The function returns ESP_OK if the initialization was
 * successful. If there was an error, it returns an appropriate ESP_ERR code.
 *
 * @param config Pointer to an `rgb_led_cfg_t` struct containing the
 * configuration data for the RGB LED.
 * @return esp_err_t ESP_OK if successful, or an error code if not.
 */
esp_err_t rgb_led_init(rgb_led_t* rgb_led_cfg) {
  esp_err_t ret = ESP_OK;

  if (!rgb_led_cfg) {
    return ESP_ERR_INVALID_ARG;
  }

  rgb_led_t led = *rgb_led_cfg;

  // Check that the pins are valid for output
  ESP_GOTO_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(led.red_pin), ESP_ERR_INVALID_ARG,
                    err, TAG, "Invalid red_pin");
  ESP_GOTO_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(led.green_pin),
                    ESP_ERR_INVALID_ARG, err, TAG, "Invalid green_pin");
  ESP_GOTO_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(led.blue_pin),
                    ESP_ERR_INVALID_ARG, err, TAG, "Invalid blue_pin");

  // Configure the GPIO pins for output
  // ESP_ERROR_CHECK(gpio_reset_pin(led.red_pin));
  ESP_ERROR_CHECK(gpio_set_direction(led.red_pin, GPIO_MODE_OUTPUT));

  // ESP_ERROR_CHECK(gpio_reset_pin(led.green_pin));
  ESP_ERROR_CHECK(gpio_set_direction(led.green_pin, GPIO_MODE_OUTPUT));

  // ESP_ERROR_CHECK(gpio_reset_pin(led.blue_pin));
  ESP_ERROR_CHECK(gpio_set_direction(led.blue_pin, GPIO_MODE_OUTPUT));

  ESP_ERROR_CHECK(gpio_set_level(led.red_pin, 0));
  ESP_ERROR_CHECK(gpio_set_level(led.green_pin, 0));
  ESP_ERROR_CHECK(gpio_set_level(led.blue_pin, 0));

  ESP_RETURN_ON_ERROR(setup_ledc(led.red_pin, led.red_channel), TAG,
                      "Failed to setup RED LED");

  // Initialize LEDC for the green LED
  ESP_RETURN_ON_ERROR(setup_ledc(led.green_pin, led.green_channel), TAG,
                      "Failed to setup GREEN LED");

  // Initialize LEDC for the blue LED
  ESP_RETURN_ON_ERROR(setup_ledc(led.blue_pin, led.blue_channel), TAG,
                      "Failed to setup BLUE LED");

  for (int i = 0; i < num_led_effects; i++) {
    if (led_effects[i].led == rgb_led_cfg) {
      ESP_LOGE(TAG, "LED already present in led_effects array");
      return ESP_ERR_INVALID_STATE;
    }
  }

  // LED not found in the led_effects array, create a new LED effect
  if (num_led_effects >= MAX_RGB_LEDS) {
    ESP_LOGE(TAG, "Maximum number of LEDs reached, unable to add LED effect.");
    return ESP_FAIL;  // Maximum number of LEDs reached, unable to add LED
                      // effect
  }

  led_effects[num_led_effects].led = rgb_led_cfg;
  num_led_effects++;

  return ESP_OK;

  // Error handling block
err:
  ESP_LOGE(TAG, "Invalid argument: %s", esp_err_to_name(ret));
  return ret;
}

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
esp_err_t rgb_led_set_color(rgb_led_t* rgb_led, uint32_t color) {
  if (!rgb_led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(rgb_led);
  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }
  rgb_led_stop_any_effect(led_effects[i].led);
  return set_color(led_effects[i].led, color);
}

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
                      ledc_channel_t blue_ch) {
  rgb_led_t led;
  led.red_pin = red_pin;
  led.green_pin = green_pin;
  led.blue_pin = blue_pin;
  led.red_channel = red_ch;
  led.green_channel = green_ch;
  led.blue_channel = blue_ch;
  return led;
}

/**
 * @brief Stops the active transition effect for a specific LED.
 *
 * This function stops the active transition effect for the specified LED. It
 * stops and deletes the associated timer, resets the transition effect data,
 * and updates the effect state to indicate that no effect is active.
 *
 * @param led Pointer to the LED structure.
 * @return
 *     - ESP_OK if the transition effect was successfully stopped.
 *     - ESP_FAIL if the transition effect is not active for the specified LED.
 */
esp_err_t rgb_led_stop_transition_effect(rgb_led_t* led) {
  if (!led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  if (led_effects[i].effect_state == RGB_LED_EFFECT_TRANSITION) {
    // Stop and delete the transition effect timer
    esp_err_t err = esp_timer_stop(led_effects[i].transition_effect.timer);
    if (err == ESP_ERR_INVALID_ARG) {
      ESP_LOGE(TAG, "Failed to stop transition effect timer: %s",
               esp_err_to_name(err));
      return err;
    }

    err = esp_timer_delete(led_effects[i].transition_effect.timer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to delete transition effect timer: %s",
               esp_err_to_name(err));
      return err;
    }

    // Reset the transition effect data and effect state
    memset(&led_effects[i].transition_effect, 0, sizeof(rgb_led_transition_t));
    led_effects[i].effect_state = RGB_LED_EFFECT_NONE;

    ESP_LOGI(TAG, "Transition effect stopped for LED");
    return ESP_OK;
  }

  ESP_LOGW(TAG, "Transition effect not active for the specified LED");
  return ESP_FAIL;
}

/**
 * @brief Stops the active blink effect for a specific LED.
 *
 * This function stops the active blink effect for the specified LED. It stops
 * and deletes the associated timer, resets the blink effect data, and updates
 * the effect state to indicate that no effect is active.
 *
 * @param led Pointer to the LED structure.
 * @return
 *     - ESP_OK if the blink effect was successfully stopped.
 *     - ESP_FAIL if the blink effect is not active for the specified LED.
 */
esp_err_t rgb_led_stop_blink_effect(rgb_led_t* led) {
  if (!led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  if (led_effects[i].effect_state == RGB_LED_EFFECT_BLINK) {
    // Stop and delete the blink effect timer
    esp_err_t err = esp_timer_stop(led_effects[i].blink_effect.timer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to stop blink effect timer: %s",
               esp_err_to_name(err));
      return err;
    }

    err = esp_timer_delete(led_effects[i].blink_effect.timer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to delete blink effect timer: %s",
               esp_err_to_name(err));
      return err;
    }

    // Reset the blink effect data and effect state
    memset(&led_effects[i].blink_effect, 0, sizeof(rgb_led_blink_t));
    led_effects[i].effect_state = RGB_LED_EFFECT_NONE;
    return ESP_OK;
  }

  ESP_LOGW(TAG, "Blink effect not active for the specified LED");
  return ESP_FAIL;
}

/**
 * @brief Stops the active breath effect for a specific LED.
 *
 * This function stops the active breath effect for the specified LED. It stops
 * and deletes the associated timer, turns off the LED, resets the breath effect
 * data, and updates the effect state to indicate that no effect is active.
 *
 * @param led Pointer to the LED structure.
 * @return
 *     - ESP_OK if the breath effect was successfully stopped.
 *     - ESP_FAIL if the breath effect is not active for the specified LED.
 */
esp_err_t rgb_led_stop_breath_effect(rgb_led_t* led) {
  if (!led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  if (led_effects[i].effect_state == RGB_LED_EFFECT_BREATH) {
    // Stop and delete the breath effect timer
    esp_err_t err = esp_timer_stop(led_effects[i].breath_effect.timer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to stop breath effect timer: %s",
               esp_err_to_name(err));
      return err;
    }

    err = esp_timer_delete(led_effects[i].breath_effect.timer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to delete breath effect timer: %s",
               esp_err_to_name(err));
      return err;
    }

    // Reset the breath effect data and effect state
    memset(&led_effects[i].breath_effect, 0, sizeof(rgb_led_breath_t));
    led_effects[i].effect_state = RGB_LED_EFFECT_NONE;

    ESP_LOGI(TAG, "Breath effect stopped for LED");
    return ESP_OK;
  }

  ESP_LOGW(TAG, "Blink effect not active for the specified LED");
  return ESP_FAIL;
}

/**
 * @brief Callback function for transitioning the LED color back to the previous
 * color.
 *
 * This callback function is called when the transition timer expires. It
 * transitions the LED color back to the previous color that was stored before
 * the transition started. The function is intended to be used internally within
 * the RGB LED library and should not be called directly.
 *
 * @param arg Pointer to the rgb_led_t structure representing the RGB LED.
 */
static void transition_effect_callback(void* arg) {
  // Cast the argument to the transition data structure
  rgb_led_transition_t* transition_data = (rgb_led_transition_t*) arg;

  // Extract the LED and previous color from the transition data
  rgb_led_t* led = transition_data->led;
  uint32_t previous_color = transition_data->color;

  // Set the LED color to the previous color
  set_color(led, previous_color);

  // Invoke the callback after transition
  if (transition_data->callback)
    transition_data->callback(led);

  // Update the flag to indicate that the color transition is no longer active
  rgb_led_stop_transition_effect(led);
}

/**
 * @brief Callback function for the breath effect.
 *
 * This function is called periodically to update the LED brightness for the
 * breath effect.
 *
 * @param arg Pointer to the argument (breath_leds struct) containing breath
 * effect data.
 */
static void breath_effect_callback(void* arg) {
  rgb_led_breath_t* breath_data = (rgb_led_breath_t*) arg;
  esp_err_t ret = ESP_OK;

  // Extract the color from the breath effect data
  uint32_t color = breath_data->color;

  // Pre-compute the brightness factor
  float brightness_factor = breath_data->brightness;

  // Extract and calculate the duty cycles directly
  uint8_t red_duty_cycle =
      (uint8_t) (brightness_factor * MIN((color >> 16), 255));
  uint8_t green_duty_cycle =
      (uint8_t) (brightness_factor * MIN(((color >> 8) & 0xFF), 255));
  uint8_t blue_duty_cycle =
      (uint8_t) (brightness_factor * MIN((color & 0xFF), 255));

  // Recombine
  color = ((uint32_t) red_duty_cycle << 16) |
          ((uint32_t) green_duty_cycle << 8) | blue_duty_cycle;

  // Set the duty cycle for each LED channel
  set_color(breath_data->led, color);

  // Adjust the brightness for the next step
  breath_data->brightness += breath_data->step;

  // Check if the breath effect should be reversed
  if (breath_data->brightness >= 1.0 || breath_data->brightness <= 0.0) {
    breath_data->step *= -1;
  }
}

/**
 * @brief Callback function for the blink effect.
 *
 * This function is called periodically to update the LED state for the blink
 * effect.
 *
 * @param arg Pointer to the argument (blink data) containing the blink effect
 * parameters.
 */
static void blink_timer_callback(void* arg) {
  rgb_led_blink_t* blink_data = (rgb_led_blink_t*) arg;

  // Extract the blink effect data
  uint32_t color = blink_data->color;
  uint8_t blink_len = blink_data->pulse_count;
  uint32_t time_on_ms = blink_data->time_on;
  uint32_t time_off_ms = blink_data->time_off;
  uint32_t time_out_ms = blink_data->time_out;

  static bool led_state = false;
  static uint8_t blink_counter = 0;

  if (led_state) {
    // Turn off the LED
    set_color(blink_data->led, COLOR_BLACK);
    led_state = false;

    // Increment the blink counter
    blink_counter++;

    // Check if the specified number of blinks has been completed
    if (blink_counter >= blink_len) {
      blink_counter = 0;  // Reset the blink counter

      if (time_out_ms != 0) {
        // Wait for time_out_ms and then restart the blink sequence
        esp_timer_start_once(blink_data->timer, time_out_ms * 1000);
        return;
      }
    }

    esp_timer_start_once(blink_data->timer, time_on_ms * 1000);
  } else {
    // Set the LED color
    set_color(blink_data->led, color);
    led_state = true;
    esp_timer_start_once(blink_data->timer, time_off_ms * 1000);
  }
}

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
                                          void (*callback)(rgb_led_t*)) {
  if (!rgb_led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(rgb_led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  if (led_effects[i].effect_state != RGB_LED_EFFECT_NONE) {
    rgb_led_stop_any_effect(rgb_led);
  }

  // Set the LED color to the new color
  set_color(rgb_led, new_color);

  led_effects[i].transition_effect.callback = callback;

  // Create a timer to transition the color back to the previous color after the
  // specified duration
  esp_timer_handle_t timer;
  esp_timer_create_args_t timer_args = {
      .callback = transition_effect_callback,
      .arg = &led_effects[i].transition_effect};
  esp_err_t err = esp_timer_create(&timer_args, &timer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(err));
    return err;
  }

  // Start the timer for the transition effect
  err = esp_timer_start_once(timer, effect_duration * 1000);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(err));
    esp_timer_delete(timer);
    return err;
  }
  // Set the transition effect data for this LED
  led_effects[i].transition_effect.led = rgb_led;
  led_effects[i].transition_effect.color = current_color;
  led_effects[i].transition_effect.timer = timer;
  led_effects[i].effect_state = RGB_LED_EFFECT_TRANSITION;

  ESP_LOGI(TAG, "Transition effect started for LED");

  return ESP_OK;
}

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
                                      uint16_t duration_ms) {
  if (!rgb_led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(rgb_led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  rgb_led_stop_any_effect(rgb_led);

  if (led_effects[i].effect_state == RGB_LED_EFFECT_NONE) {
    // Create and start the precision timer for the breath effect
    esp_timer_handle_t timer;

    esp_timer_create_args_t timer_args = {.callback = breath_effect_callback,
                                          .arg = &led_effects[i].breath_effect,
                                          .name = "breath_effect_timer",
                                          .dispatch_method = ESP_TIMER_TASK};
    esp_err_t err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(err));
      return err;
    }

    err = esp_timer_start_periodic(timer, duration_ms * 1000);
    if (err != ESP_OK) {
      esp_timer_delete(timer);
      ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(err));
      return err;
    }

    // Set the breath effect data for this LED
    led_effects[i].breath_effect.step =
        0.05;  // Step size for increasing/decreasing brightness
    led_effects[i].breath_effect.led = rgb_led;
    led_effects[i].breath_effect.color = color;
    led_effects[i].breath_effect.timer = timer;
    led_effects[i].breath_effect.brightness = 0;
    led_effects[i].effect_state = RGB_LED_EFFECT_BREATH;
    ESP_LOGI(TAG, "Started breath effect for LED");

    return ESP_OK;
  }

  return ESP_FAIL;
}

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
                                     uint32_t time_out) {
  if (!rgb_led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(rgb_led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  switch (led_effects[i].effect_state) {
    case RGB_LED_EFFECT_BLINK:
      // Update the existing blink effect
      led_effects[i].blink_effect.color = color;
      led_effects[i].blink_effect.pulse_count = pulse_count;
      led_effects[i].blink_effect.time_on = time_on;
      led_effects[i].blink_effect.time_off = time_off;
      led_effects[i].blink_effect.time_out = time_out;
      return ESP_OK;
    case RGB_LED_EFFECT_TRANSITION:
      // Stop the transition effect before starting the blink effect
      rgb_led_stop_transition_effect(rgb_led);
      break;
    case RGB_LED_EFFECT_BREATH:
      // Stop the breath effect before starting the blink effect
      rgb_led_stop_breath_effect(rgb_led);
      break;
    default:
      break;
  }

  if (led_effects[i].effect_state == RGB_LED_EFFECT_NONE) {
    // Create and start the precision timer for the blink effect
    esp_timer_handle_t timer;
    esp_timer_create_args_t timer_args = {.callback = blink_timer_callback,
                                          .arg = &led_effects[i].blink_effect,
                                          .name = "blink_effect_timer",
                                          .dispatch_method = ESP_TIMER_TASK};
    esp_err_t err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(err));
      return err;
    }

    // Set the blink effect data for this LED
    led_effects[i].led = rgb_led;
    led_effects[i].blink_effect.led = rgb_led;
    led_effects[i].blink_effect.color = color;
    led_effects[i].blink_effect.pulse_count = pulse_count;
    led_effects[i].blink_effect.time_on = time_on;
    led_effects[i].blink_effect.time_off = time_off;
    led_effects[i].blink_effect.time_out = time_out;
    led_effects[i].blink_effect.timer = timer;
    led_effects[i].effect_state = RGB_LED_EFFECT_BLINK;

    // Start the blink effect
    esp_timer_start_once(timer, time_on * 1000);
    return ESP_OK;
  }

  return ESP_FAIL;
}

/**
 * @brief Stops the active effect for a specific LED.
 *
 * @param led Pointer to the LED structure.
 * @return ESP_OK if the effect was successfully stopped, ESP_FAIL otherwise.
 */
esp_err_t rgb_led_stop_any_effect(rgb_led_t* rgb_led) {
  if (!rgb_led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int index = get_led_index(rgb_led);

  if (index == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  switch (led_effects[index].effect_state) {
    case RGB_LED_EFFECT_TRANSITION:
      // Stop and clean up the transition effect for the LED
      rgb_led_stop_transition_effect(rgb_led);
      break;
    case RGB_LED_EFFECT_BREATH:
      // Stop and clean up the breath effect for the LED
      rgb_led_stop_breath_effect(rgb_led);
      // ...
      break;
    case RGB_LED_EFFECT_BLINK:
      // Stop and clean up the blink effect for the LED
      rgb_led_stop_blink_effect(rgb_led);
      break;
    default:
  }
  led_effects[index].effect_state = RGB_LED_EFFECT_NONE;
  return ESP_OK;
}

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
esp_err_t rgb_led_deinit(rgb_led_t* rgb_led_cfg) {
  if (!rgb_led_cfg) {
    return ESP_ERR_INVALID_ARG;
  }

  for (int i = 0; i < num_led_effects; i++) {
    if (led_effects[i].led == rgb_led_cfg) {
      switch (led_effects[i].effect_state) {
        case RGB_LED_EFFECT_TRANSITION:
          rgb_led_stop_transition_effect(rgb_led_cfg);
          break;
        case RGB_LED_EFFECT_BREATH:
          rgb_led_stop_breath_effect(rgb_led_cfg);
          break;
        case RGB_LED_EFFECT_BLINK:
          rgb_led_stop_blink_effect(rgb_led_cfg);
          break;
        default:
          break;
      }

      // Remove the LED from the led_effects array
      num_led_effects--;
      led_effects[i] = led_effects[num_led_effects];
      memset(&led_effects[num_led_effects], 0, sizeof(rgb_led_effects_t));
      return ESP_OK;
    }
  }

  return ESP_FAIL;  // LED not found in the led_effects array
}

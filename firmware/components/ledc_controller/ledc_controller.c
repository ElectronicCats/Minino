#include "ledc_controller.h"
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

static const char* TAG = "LED_CONTROLLER";

#define LEDC_TIMER     LEDC_TIMER_0
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES  LEDC_TIMER_8_BIT  // Set duty resolution to 8 bits
#define LEDC_FREQUENCY (4000)  // Frequency in Hertz. Set frequency at 4 kHz

// LED Effect State Enum
// This enum represents the different states of the LED effects.
typedef enum {
  LED_EFFECT_NONE,    // No effect active
  LED_EFFECT_BREATH,  // Breath effect active
  LED_EFFECT_BLINK    // Blink effect active
} led_effect_state_t;

// LED Breath Structure
// This structure represents the breath effect data for a LED.
typedef struct {
  led_t* led;                // Pointer to the LED
  esp_timer_handle_t timer;  // Timer handle for the breath effect
  float brightness;          // Current brightness of the LED
  float step;                // Step size for increasing/decreasing brightness
} led_breath_t;

// LED Blink Structure
// This structure represents the blink effect data for a LED.
typedef struct {
  led_t* led;                // Pointer to the LED
  uint8_t duty;              // Led intensity for blink effect
  esp_timer_handle_t timer;  // Timer handle for the blink effect
  uint32_t time_on;          // Duration of the LED ON state during each pulse
  uint32_t time_off;         // Duration of the LED OFF state during each pulse
  uint32_t time_out;         // Timeout duration for the blink effect
  uint8_t pulse_count;       // Number of pulses for the blink effect
} led_blink_t;

// LED Effects Structure
// This structure represents the effects data for an RGB LED.
typedef struct {
  led_t* led;                       // Pointer to the RGB LED
  led_effect_state_t effect_state;  // Current effect state
  led_breath_t breath_effect;       // Breath effect data
  led_blink_t blink_effect;         // Blink effect data
  // Add any additional fields you need for other effects
} led_effects_t;

// Maximum number of LEDs for breath effect
#define MAX_LEDS 2  // max 8 ledc_channels

// Array to hold the breath and blink effect data for multiple LEDs
static led_effects_t led_effects[MAX_LEDS];

// Number of breath and blink effect LEDs currently active
static int num_led_effects = 0;

static int get_led_index(led_t* led) {
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
 * predefined configuration (8-bit duty resolution, 4000 Hz frequency,
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
      .speed_mode = LEDC_MODE,
      .duty_resolution = LEDC_DUTY_RES,
      .timer_num = LEDC_TIMER,
      .freq_hz = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
      .clk_cfg = LEDC_AUTO_CLK};

  // Set the configuration
  esp_err_t err = ledc_timer_config(&ledc_timer);
  if (err != ESP_OK)
    return err;

  // Prepare individual configuration for each LED
  ledc_channel_config_t ledc_channel_cfg = {.speed_mode = LEDC_MODE,
                                            .channel = led_channel,
                                            .timer_sel = LEDC_TIMER,
                                            .intr_type = LEDC_INTR_DISABLE,
                                            .gpio_num = led_gpio,
                                            .duty = 0,  // Set duty to 0%
                                            .hpoint = 0};

  ledc_fade_func_install(NULL);

  // Set the configuration
  return ledc_channel_config(&ledc_channel_cfg);
}

/**
 * @brief Set the duty cycle of a PWM LED
 *
 * @description This function sets the duty of a PWM LED. The
 * duty is specified as a 8-bit PWM duty value.If an
 * error occurs while setting the duty cycle, an appropriate ESP_ERR code is
 * returned.
 *
 * @param led A pointer to the led_t structure that contains the
 * configuration of the LED.
 * @param duty The 8-bit duty cycle value.
 * @return esp_err_t ESP_OK if successful, or an error code if not.
 */
static esp_err_t set_duty(led_t* led, uint8_t duty) {
  if (!led) {
    return ESP_ERR_INVALID_ARG;
  }
  ESP_RETURN_ON_ERROR(
      ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, led->channel, duty, 0), TAG,
      "Failed to set red duty");

  return ESP_OK;
}

esp_err_t led_controller_led_init(led_t* led_cfg) {
#if !defined(CONFIG_LEDC_CONTROLLER_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  esp_err_t ret = ESP_OK;

  if (!led_cfg) {
    return ESP_ERR_INVALID_ARG;
  }

  led_t led = *led_cfg;

  // Check that the pin is valid for output
  ESP_GOTO_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(led.pin), ESP_ERR_INVALID_ARG,
                    err, TAG, "Invalid red_pin");

  // Configure the GPIO pin for output
  ESP_ERROR_CHECK(gpio_set_direction(led.pin, GPIO_MODE_OUTPUT));

  ESP_ERROR_CHECK(gpio_set_level(led.pin, 0));

  ESP_RETURN_ON_ERROR(setup_ledc(led.pin, led.channel), TAG,
                      "Failed to setup LED");

  for (int i = 0; i < num_led_effects; i++) {
    if (led_effects[i].led == led_cfg) {
      ESP_LOGE(TAG, "LED already present in led_effects array");
      return ESP_ERR_INVALID_STATE;
    }
  }

  // LED not found in the led_effects array, create a new LED effect
  if (num_led_effects >= MAX_LEDS) {
    ESP_LOGE(TAG, "Maximum number of LEDs reached, unable to add LED effect.");
    return ESP_FAIL;  // Maximum number of LEDs reached, unable to add LED
                      // effect
  }

  led_effects[num_led_effects].led = led_cfg;
  num_led_effects++;

  return ESP_OK;

  // Error handling block
err:
  ESP_LOGE(TAG, "Invalid argument: %s", esp_err_to_name(ret));
  return ret;
}

led_t led_controller_led_new(gpio_num_t pin, ledc_channel_t ch) {
  led_t led;
  led.pin = pin;
  led.channel = ch;
  return led;
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
esp_err_t led_stop_blink_effect(led_t* led) {
  if (!led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }
  if (led_effects[i].effect_state == LED_EFFECT_BLINK) {
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
    memset(&led_effects[i].blink_effect, 0, sizeof(led_blink_t));
    led_effects[i].effect_state = LED_EFFECT_NONE;
    ESP_LOGI(TAG, "Blink effect stopped for LED");
    set_duty(led, 0);
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
esp_err_t led_stop_breath_effect(led_t* led) {
  if (!led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  if (led_effects[i].effect_state == LED_EFFECT_BREATH) {
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
    memset(&led_effects[i].breath_effect, 0, sizeof(led_breath_t));
    led_effects[i].effect_state = LED_EFFECT_NONE;

    ESP_LOGI(TAG, "Breath effect stopped for LED");
    set_duty(led, 0);
    return ESP_OK;
  }

  ESP_LOGW(TAG, "Blink effect not active for the specified LED");
  return ESP_FAIL;
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
  led_breath_t* breath_data = (led_breath_t*) arg;
  esp_err_t ret = ESP_OK;

  // Pre-compute the brightness factor
  float brightness_factor = breath_data->brightness;

  // Extract and calculate the duty cycles directly
  uint8_t duty_cycle = (uint8_t) (brightness_factor * 255);

  // Set the duty cycle for each LED channel
  set_duty(breath_data->led, duty_cycle);

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
  led_blink_t* blink_data = (led_blink_t*) arg;

  // Extract the blink effect data
  uint8_t duty = blink_data->duty;
  uint8_t blink_len = blink_data->pulse_count;
  uint32_t time_on_ms = blink_data->time_on;
  uint32_t time_off_ms = blink_data->time_off;
  uint32_t time_out_ms = blink_data->time_out;

  static bool led_state = false;
  static uint8_t blink_counter = 0;
  if (led_state) {
    // Turn off the LED
    set_duty(blink_data->led, 0);
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

    esp_timer_start_once(blink_data->timer, time_off_ms * 1000);
  } else {
    // Set the LED color
    set_duty(blink_data->led, duty);
    led_state = true;
    esp_timer_start_once(blink_data->timer, time_on_ms * 1000);
  }
}

esp_err_t led_controller_start_breath_effect(led_t* led, uint16_t duration_ms) {
  if (!led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }
  led_controller_stop_any_effect(led);

  if (led_effects[i].effect_state == LED_EFFECT_NONE) {
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
    led_effects[i].breath_effect.led = led;
    led_effects[i].breath_effect.timer = timer;
    led_effects[i].breath_effect.brightness = 0;
    led_effects[i].effect_state = LED_EFFECT_BREATH;
    ESP_LOGI(TAG, "Started breath effect for LED");

    return ESP_OK;
  }

  return ESP_FAIL;
}

esp_err_t led_controller_start_blink_effect(led_t* led,
                                            uint8_t duty,
                                            uint8_t pulse_count,
                                            uint32_t time_on,
                                            uint32_t time_off,
                                            uint32_t time_out) {
  if (!led) {
    ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int i = get_led_index(led);

  if (i == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  switch (led_effects[i].effect_state) {
    case LED_EFFECT_BLINK:
      // Update the existing blink effect
      led_effects[i].blink_effect.duty = duty;
      led_effects[i].blink_effect.pulse_count = pulse_count;
      led_effects[i].blink_effect.time_on = time_on;
      led_effects[i].blink_effect.time_off = time_off;
      led_effects[i].blink_effect.time_out = time_out;
      return ESP_OK;
    case LED_EFFECT_BREATH:
      // Stop the breath effect before starting the blink effect
      led_stop_breath_effect(led);
      break;
    default:
      break;
  }

  if (led_effects[i].effect_state == LED_EFFECT_NONE) {
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
    led_effects[i].led = led;
    led_effects[i].blink_effect.led = led;
    led_effects[i].blink_effect.duty = duty;
    led_effects[i].blink_effect.pulse_count = pulse_count;
    led_effects[i].blink_effect.time_on = time_on;
    led_effects[i].blink_effect.time_off = time_off;
    led_effects[i].blink_effect.time_out = time_out;
    led_effects[i].blink_effect.timer = timer;
    led_effects[i].effect_state = LED_EFFECT_BLINK;

    // Start the blink effect
    esp_timer_start_once(timer, time_off * 1000);
    ESP_LOGI(TAG, "Started blink effect for LED");
    return ESP_OK;
  }

  return ESP_FAIL;
}

esp_err_t led_controller_stop_any_effect(led_t* led) {
  if (!led) {
    // ESP_LOGE(TAG, "Invalid LED pointer");
    return ESP_ERR_INVALID_ARG;
  }

  int index = get_led_index(led);

  if (index == -1) {
    ESP_LOGE(TAG, "LED not found");
    return ESP_FAIL;
  }

  switch (led_effects[index].effect_state) {
    case LED_EFFECT_BREATH:
      // Stop and clean up the breath effect for the LED
      led_stop_breath_effect(led);
      // ...
      break;
    case LED_EFFECT_BLINK:
      // Stop and clean up the blink effect for the LED
      led_stop_blink_effect(led);
      break;
    default:
      set_duty(led, 0);
  }
  led_effects[index].effect_state = LED_EFFECT_NONE;
  return ESP_OK;
}

esp_err_t led_controller_led_deinit(led_t* led_cfg) {
  if (!led_cfg) {
    return ESP_ERR_INVALID_ARG;
  }

  for (int i = 0; i < num_led_effects; i++) {
    if (led_effects[i].led == led_cfg) {
      switch (led_effects[i].effect_state) {
        case LED_EFFECT_BREATH:
          led_stop_breath_effect(led_cfg);
          break;
        case LED_EFFECT_BLINK:
          led_stop_blink_effect(led_cfg);
          break;
        default:
          break;
      }

      // Remove the LED from the led_effects array
      num_led_effects--;
      led_effects[i] = led_effects[num_led_effects];
      memset(&led_effects[num_led_effects], 0, sizeof(led_effects_t));
      return ESP_OK;
    }
  }

  return ESP_FAIL;  // LED not found in the led_effects array
}

void led_controller_led_on(led_t* led) {
  led_controller_stop_any_effect(led);
  set_duty(led, 255);
}

void led_controller_led_off(led_t* led) {
  led_controller_stop_any_effect(led);
}

void led_controller_set_duty(led_t* led, uint8_t duty) {
  led_controller_stop_any_effect(led);
  set_duty(led, duty);
}

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int esp_err_t;
#define ESP_OK                0
#define ESP_FAIL              -1
#define ESP_ERR_INVALID_ARG   -2
#define ESP_ERR_NO_MEM        -3
#define ESP_ERR_INVALID_STATE -4

typedef enum {
  BUTTON_TYPE_GPIO = 0,
  BUTTON_TYPE_ADC = 1,
} button_type_t;

typedef enum {
  BUTTON_ACTIVE_HIGH = 0,
  BUTTON_ACTIVE_LOW = 1,
} button_active_level_t;

typedef struct {
  button_type_t type;
  union {
    struct {
      uint32_t gpio_num;
      button_active_level_t active_level;
      bool enable_power_save;
      uint32_t disable_pull;
    } gpio_button_config;
    struct {
      uint32_t adc_channel;
      uint16_t button_index;
      uint32_t min;
      uint32_t max;
    } adc_button_config;
  };
} button_config_t;

typedef void* button_handle_t;

typedef enum {
  BUTTON_PRESS_DOWN = 0,
  BUTTON_PRESS_UP = 1,
  BUTTON_PRESS_REPEAT = 2,
  BUTTON_PRESS_REPEAT_DONE = 3,
  BUTTON_SINGLE_CLICK = 4,
  BUTTON_DOUBLE_CLICK = 5,
  BUTTON_MULTIPLE_CLICK = 6,
  BUTTON_LONG_PRESS_START = 7,
  BUTTON_LONG_PRESS_HOLD = 8,
  BUTTON_LONG_PRESS_UP = 9,
  BUTTON_PRESS_END = 10,
  BUTTON_NONE_PRESS = 11,
} button_event_t;

typedef void (*button_cb_t)(void* arg, void* data);

esp_err_t iot_button_create(const button_config_t* config,
                            button_handle_t* btn);
esp_err_t iot_button_register_cb(button_handle_t btn,
                                 button_event_t event,
                                 button_cb_t cb,
                                 void* arg);
esp_err_t iot_button_delete(button_handle_t btn);

#ifdef __cplusplus
}
#endif
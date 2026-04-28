#pragma once

#include "esp_zigbee_core.h"

#define HA_ONOFF_LIGHT_ENDPOINT    10
#define ZIGBEE_LIGHT_MAX_CHILDREN  10
#define INSTALLCODE_POLICY_ENABLE_LIGHT false

#define ESP_ZB_ZR_CONFIG()                              \
  {                                                     \
      .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,         \
      .install_code_policy = INSTALLCODE_POLICY_ENABLE_LIGHT, \
      .nwk_cfg.zczr_cfg =                               \
          {                                             \
              .max_children = ZIGBEE_LIGHT_MAX_CHILDREN, \
          },                                            \
  }

#define ESP_ZB_LIGHT_RADIO_CONFIG()   \
  {                                   \
      .radio_mode = ZB_RADIO_MODE_NATIVE, \
  }

#define ESP_ZB_LIGHT_HOST_CONFIG()                          \
  {                                                         \
      .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, \
  }

typedef enum {
  LIGHT_DISPLAY_JOINING = 0,
  LIGHT_DISPLAY_JOINING_FAILED,
  LIGHT_DISPLAY_ON,
  LIGHT_DISPLAY_OFF,
  LIGHT_DISPLAY_LEAVING,
} light_display_status_t;

typedef void (*light_display_cb_t)(uint8_t);

void zigbee_light_init(void);
void zigbee_light_deinit(void);
void zigbee_light_set_display_cb(light_display_cb_t cb);
void zigbee_light_app_signal_handler(esp_zb_app_signal_t* signal_struct);
bool zigbee_light_is_joined(void);
bool zigbee_light_get_state(void);

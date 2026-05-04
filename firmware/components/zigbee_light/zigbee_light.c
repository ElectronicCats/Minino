#include "zigbee_light.h"

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "leds.h"
#include "radio_selector.h"

#define TAG "zigbee_light"

typedef enum {
  LIGHT_INIT = 0,
  LIGHT_JOINING,
  LIGHT_JOINED,
  LIGHT_JOIN_FAILED,
  LIGHT_DISCONNECTED,
  LIGHT_EXIT,
} light_state_t;

static volatile light_state_t light_state = LIGHT_INIT;
static light_state_t light_state_prev = LIGHT_EXIT;
static volatile bool light_on = false;

static light_display_cb_t zigbee_light_display_cb = NULL;
static TaskHandle_t light_joining_task_handle = NULL;
static TaskHandle_t light_state_machine_task_handle = NULL;

bool zigbee_light_is_joined(void) {
  return light_state == LIGHT_JOINED;
}

bool zigbee_light_get_state(void) {
  return light_on;
}

void zigbee_light_set_display_cb(light_display_cb_t cb) {
  zigbee_light_display_cb = cb;
}

static esp_err_t zb_attribute_handler(
    const esp_zb_zcl_set_attr_value_message_t* message) {
  if (!message || message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    return ESP_ERR_INVALID_ARG;
  }
  if (message->info.dst_endpoint == HA_ONOFF_LIGHT_ENDPOINT &&
      message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF &&
      message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
      message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
    light_on = message->attribute.data.value
                   ? *(bool*) message->attribute.data.value
                   : false;
    if (light_on) {
      leds_on();
    } else {
      leds_off();
    }
    ESP_LOGI(TAG, "Light turned %s", light_on ? "ON" : "OFF");
    if (zigbee_light_display_cb) {
      zigbee_light_display_cb(light_on ? LIGHT_DISPLAY_ON : LIGHT_DISPLAY_OFF);
    }
  }
  return ESP_OK;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id,
                                   const void* message) {
  if (callback_id == ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID) {
    return zb_attribute_handler(
        (const esp_zb_zcl_set_attr_value_message_t*) message);
  }
  return ESP_OK;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  light_state = LIGHT_JOINING;
  esp_err_t err = esp_zb_bdb_start_top_level_commissioning(mode_mask);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start commissioning (status: %s)",
             esp_err_to_name(err));
  }
}

static void __attribute__((unused)) restart_steering_after_disconnect_cb(uint8_t mode_mask) {
  bdb_start_top_level_commissioning_cb(mode_mask);
}

static void handle_network_disconnected(void) {
  if (light_state == LIGHT_DISCONNECTED || light_state == LIGHT_EXIT) {
    return;
  }
  light_state = LIGHT_DISCONNECTED;
  leds_off();
  ESP_LOGW(TAG, "Signal lost with Switch - Restarting search");
  if (zigbee_light_display_cb) {
    zigbee_light_display_cb(LIGHT_DISPLAY_DISCONNECTED);
  }

  /* Force leave the network locally to clear state and association */
  esp_zb_zdo_mgmt_leave_req_param_t leave_req;
  memset(&leave_req, 0, sizeof(esp_zb_zdo_mgmt_leave_req_param_t));
  leave_req.dst_nwk_addr = esp_zb_get_short_address();
  leave_req.rejoin = false;
  leave_req.remove_children = false;
  esp_zb_get_long_address(leave_req.device_address);

  esp_zb_zdo_device_leave_req(&leave_req, NULL, NULL);

  /* Schedule a fresh search after a short delay to let the stack settle */
  esp_zb_scheduler_alarm(
      (esp_zb_callback_t) bdb_start_top_level_commissioning_cb,
      ESP_ZB_BDB_MODE_NETWORK_STEERING, 2000);
}

static void light_schedule_coordinator_ping(uint8_t param);

static void light_coordinator_ping_cb(esp_zb_zdp_status_t zdo_status,
                                      esp_zb_zdo_ieee_addr_rsp_t* resp,
                                      void* user_ctx) {
  if (light_state != LIGHT_JOINED) {
    return;
  }
  if (zdo_status != ESP_ZB_ZDP_STATUS_SUCCESS) {
    ESP_LOGW(TAG, "Coordinator not responding (0x%02x)", zdo_status);
    handle_network_disconnected();
    return;
  }
  esp_zb_scheduler_alarm((esp_zb_callback_t) light_schedule_coordinator_ping, 0,
                         10000);
}

static void light_schedule_coordinator_ping(uint8_t param) {
  if (light_state != LIGHT_JOINED) {
    return;
  }
  esp_zb_zdo_ieee_addr_req_param_t req = {
      .dst_nwk_addr = 0x0000,
      .addr_of_interest = 0x0000,
      .request_type = 0,
      .start_index = 0,
  };
  esp_zb_zdo_ieee_addr_req(&req, light_coordinator_ping_cb, NULL);
}

void zigbee_light_app_signal_handler(esp_zb_app_signal_t* signal_struct) {
  uint32_t* p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = *p_sg_p;

  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      ESP_LOGI(TAG, "Zigbee stack initialized");
      bdb_start_top_level_commissioning_cb(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      if (err_status == ESP_OK) {
        ESP_LOGI(TAG, "Starting network steering to join existing network");
        /* Visual feedback for scanning */
        led_start_blink(LED_LEFT, 255, 3, 50, 50, 150);
        led_start_blink(LED_RIGHT, 255, 3, 50, 50, 150);
        bdb_start_top_level_commissioning_cb(ESP_ZB_BDB_MODE_NETWORK_STEERING);
      } else {
        ESP_LOGE(TAG, "Zigbee stack init failed (status: %s)",
                 esp_err_to_name(err_status));
        light_state = LIGHT_JOIN_FAILED;
        leds_off();
      }
      break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
      if (err_status == ESP_OK) {
        ESP_LOGI(TAG,
                 "Joined network (PAN ID: 0x%04hx, Channel: %d, Addr: 0x%04hx)",
                 esp_zb_get_pan_id(), esp_zb_get_current_channel(),
                 esp_zb_get_short_address());
        light_state = LIGHT_JOINED;
        light_on = false;
        leds_off();
        if (zigbee_light_display_cb) {
          zigbee_light_display_cb(LIGHT_DISPLAY_OFF);
        }
        esp_zb_scheduler_alarm(
            (esp_zb_callback_t) light_schedule_coordinator_ping, 0, 10000);
      } else {
        ESP_LOGW(TAG, "Network steering failed (status: %s)",
                 esp_err_to_name(err_status));
        leds_off();
        /* Retry steering after 5 seconds */
        esp_zb_scheduler_alarm(
            (esp_zb_callback_t) bdb_start_top_level_commissioning_cb,
            ESP_ZB_BDB_MODE_NETWORK_STEERING, 5000);
      }
      break;

    case ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT:
      if (light_state == LIGHT_JOINED) {
        ESP_LOGW(TAG, "Parent/coordinator lost (No active links)");
        handle_network_disconnected();
      }
      break;

    case ESP_ZB_NLME_STATUS_INDICATION:
      if (light_state == LIGHT_JOINED && err_status != ESP_OK) {
        ESP_LOGW(TAG, "Network status error (0x%x) - checking connection",
                 err_status);
        handle_network_disconnected();
      }
      break;

    case ESP_ZB_ZDO_DEVICE_UNAVAILABLE:
      if (light_state == LIGHT_JOINED) {
        ESP_LOGW(TAG, "Device unavailable signal received");
        handle_network_disconnected();
      }
      break;

    case ESP_ZB_ZDO_SIGNAL_LEAVE:
      if (light_state == LIGHT_EXIT || light_state == LIGHT_DISCONNECTED) {
        ESP_LOGI(TAG, "Left network (Intentional or local action)");
        break;
      }
      ESP_LOGW(TAG, "Removed from network");
      handle_network_disconnected();
      break;

    default:
      ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s",
               esp_zb_zdo_signal_to_string(sig_type), sig_type,
               esp_err_to_name(err_status));
      break;
  }
}

/* Animates "Joining..." dots while searching for a network */
static void light_joining_task(void* pvParameters) {
  while (true) {
    if (light_state == LIGHT_JOINING && zigbee_light_display_cb) {
      zigbee_light_display_cb(LIGHT_DISPLAY_JOINING);
    }
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

static void light_state_machine_task(void* pvParameters) {
  while (true) {
    if (light_state != light_state_prev) {
      switch (light_state) {
        case LIGHT_JOIN_FAILED:
          if (zigbee_light_display_cb) {
            zigbee_light_display_cb(LIGHT_DISPLAY_JOINING_FAILED);
          }
          break;
        case LIGHT_DISCONNECTED:
          if (zigbee_light_display_cb) {
            zigbee_light_display_cb(LIGHT_DISPLAY_DISCONNECTED);
          }
          break;
        default:
          break;
      }
      light_state_prev = light_state;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

static void esp_zb_light_task(void* pvParameters) {
  esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZR_CONFIG();
  if (!radio_selector_is_stack_initialized()) {
    esp_zb_init(&zb_nwk_cfg);
    radio_selector_set_stack_initialized(true);
  }

  esp_zb_on_off_light_cfg_t light_cfg = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
  esp_zb_ep_list_t* ep_list =
      esp_zb_on_off_light_ep_create(HA_ONOFF_LIGHT_ENDPOINT, &light_cfg);
  esp_zb_device_register(ep_list);
  esp_zb_core_action_handler_register(zb_action_handler);
  esp_zb_set_primary_network_channel_set(ESP_ZB_LIGHT_PRIMARY_CHANNEL_MASK);
  ESP_ERROR_CHECK(esp_zb_start(false));
  esp_zb_stack_main_loop();
}

void zigbee_light_init(void) {
  if (!radio_selector_is_platform_configured()) {
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_LIGHT_RADIO_CONFIG(),
        .host_config = ESP_ZB_LIGHT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    radio_selector_set_platform_configured(true);
  }
  light_state = LIGHT_JOINING;
  light_state_prev = LIGHT_EXIT;
  light_on = false;

  xTaskCreate(esp_zb_light_task, "zigbee_light_main", 10240, NULL, 5, NULL);
  xTaskCreate(light_joining_task, "light_joining", 4096, NULL, 5,
              &light_joining_task_handle);
  xTaskCreate(light_state_machine_task, "light_state_machine", 4096, NULL, 5,
              &light_state_machine_task_handle);
}

void zigbee_light_deinit(void) {
  light_state = LIGHT_EXIT;
  vTaskDelay(pdMS_TO_TICKS(50));
  if (zigbee_light_display_cb) {
    zigbee_light_display_cb(LIGHT_DISPLAY_LEAVING);
  }
  if (light_joining_task_handle) {
    vTaskDelete(light_joining_task_handle);
    light_joining_task_handle = NULL;
  }
  if (light_state_machine_task_handle) {
    vTaskDelete(light_state_machine_task_handle);
    light_state_machine_task_handle = NULL;
  }
  zigbee_light_display_cb = NULL;
  esp_zb_factory_reset();
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
}

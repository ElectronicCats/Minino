/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: LicenseRef-Included
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Espressif
 * Systems integrated circuit in a product or a software update for such
 * product, must reproduce the above copyright notice, this list of conditions
 * and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * 4. Any software provided in binary form under this license must not be
 * reverse engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "zigbee_switch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "keyboard.h"
#include "menu_screens_modules.h"
#include "preferences.h"
#include "screen_modules.h"
#include "string.h"
#include "zigbee_screens_module.h"

typedef struct light_bulb_device_params_s {
  esp_zb_ieee_addr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
} light_bulb_device_params_t;

bool light_found = false;
bool network_failed = false;
bool wait_for_devices = false;
TaskHandle_t network_failed_task_handle = NULL;
TaskFunction_t wait_for_devices_task_handle = NULL;

static const char* TAG = "ESP_ZB_ON_OFF_SWITCH";

void zigbee_switch_toggle() {
  /* implemented light switch toggle functionality */
  esp_zb_zcl_on_off_cmd_t cmd_req;
  cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
  cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
  ESP_EARLY_LOGI(TAG, "Send 'on_off toggle' command");
  esp_zb_zcl_on_off_cmd_req(&cmd_req);
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

static void bind_cb(esp_zb_zdp_status_t zdo_status, void* user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    light_found = true;
    wait_for_devices = false;
    zigbee_screens_module_toggle_released();
    ESP_LOGI(TAG, "Bound successfully!");
    if (user_ctx) {
      light_bulb_device_params_t* light =
          (light_bulb_device_params_t*) user_ctx;
      ESP_LOGI(TAG, "The light originating from address(0x%x) on endpoint(%d)",
               light->short_addr, light->endpoint);
      free(light);
    }
  }
}

static void user_find_cb(esp_zb_zdp_status_t zdo_status,
                         uint16_t addr,
                         uint8_t endpoint,
                         void* user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    ESP_LOGI(TAG, "Found light");
    esp_zb_zdo_bind_req_param_t bind_req;
    light_bulb_device_params_t* light = (light_bulb_device_params_t*) malloc(
        sizeof(light_bulb_device_params_t));
    light->endpoint = endpoint;
    light->short_addr = addr;
    esp_zb_ieee_address_by_short(light->short_addr, light->ieee_addr);
    esp_zb_get_long_address(bind_req.src_address);
    bind_req.src_endp = HA_ONOFF_SWITCH_ENDPOINT;
    bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
    bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    memcpy(bind_req.dst_address_u.addr_long, light->ieee_addr,
           sizeof(esp_zb_ieee_addr_t));
    bind_req.dst_endp = endpoint;
    bind_req.req_dst_addr = esp_zb_get_short_address();
    ESP_LOGI(TAG, "Try to bind On/Off");
    esp_zb_zdo_device_bind_req(&bind_req, bind_cb, (void*) light);
  }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t* signal_struct) {
  uint32_t* p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = *p_sg_p;
  esp_zb_zdo_signal_device_annce_params_t* dev_annce_params = NULL;
  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      ESP_LOGI(TAG, "Zigbee stack initialized");
      zigbee_screens_module_creating_network();
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      if (err_status == ESP_OK) {
        ESP_LOGI(TAG, "Device started up in %s factory-reset mode",
                 esp_zb_bdb_is_factory_new() ? "" : "non");
        if (esp_zb_bdb_is_factory_new()) {
          ESP_LOGI(TAG, "Start network formation");
          esp_zb_bdb_start_top_level_commissioning(
              ESP_ZB_BDB_MODE_NETWORK_FORMATION);
        } else {
          ESP_LOGI(TAG, "Device rebooted");
        }
      } else {
        ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %s)",
                 esp_err_to_name(err_status));
      }
      break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
      if (err_status == ESP_OK) {
        esp_zb_ieee_addr_t extended_pan_id;
        esp_zb_get_extended_pan_id(extended_pan_id);
        ESP_LOGI(TAG,
                 "Formed network successfully (Extended PAN ID: "
                 "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, "
                 "Channel:%d, Short Address: 0x%04hx)",
                 extended_pan_id[7], extended_pan_id[6], extended_pan_id[5],
                 extended_pan_id[4], extended_pan_id[3], extended_pan_id[2],
                 extended_pan_id[1], extended_pan_id[0], esp_zb_get_pan_id(),
                 esp_zb_get_current_channel(), esp_zb_get_short_address());
        esp_zb_bdb_start_top_level_commissioning(
            ESP_ZB_BDB_MODE_NETWORK_STEERING);
      } else {
        ESP_LOGI(TAG, "Restart network formation (status: %s)",
                 esp_err_to_name(err_status));
        esp_zb_scheduler_alarm(
            (esp_zb_callback_t) bdb_start_top_level_commissioning_cb,
            ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
      }
      break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
      if (err_status == ESP_OK) {
        ESP_LOGI(TAG, "Network steering started");
        wait_for_devices = true;
        display_clear();
      }
      break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
      dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t*)
          esp_zb_app_signal_get_params(p_sg_p);
      ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)",
               dev_annce_params->device_short_addr);
      esp_zb_zdo_match_desc_req_param_t cmd_req;
      cmd_req.dst_nwk_addr = dev_annce_params->device_short_addr;
      cmd_req.addr_of_interest = dev_annce_params->device_short_addr;
      esp_zb_zdo_find_on_off_light(&cmd_req, user_find_cb, NULL);
      break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
      if (err_status == ESP_OK) {
        if (*(uint8_t*) esp_zb_app_signal_get_params(p_sg_p)) {
          network_failed = false;
          ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds",
                   esp_zb_get_pan_id(),
                   *(uint8_t*) esp_zb_app_signal_get_params(p_sg_p));
        } else {
          network_failed = true;
          ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.",
                   esp_zb_get_pan_id());
        }
      }
      break;
    default:
      ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s",
               esp_zb_zdo_signal_to_string(sig_type), sig_type,
               esp_err_to_name(err_status));
      break;
  }
}

/**
 * @brief Task to check if the network creation failed
 *
 * @param pvParameters
 *
 * @return void
 */
void network_failed_task(void* pvParameters) {
  while (true) {
    if (network_failed) {
      // Wait for 2 seconds to check if the network creation failed
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      if (network_failed && !light_found) {
        zigbee_screens_module_creating_network_failed();
        vTaskDelete(NULL);
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Task to show the waiting for devices screen
 *
 * @param pvParameters
 *
 * @return void
 */
void wait_for_devices_task(void* pvParameters) {
  uint8_t dots = 0;
  while (true) {
    if (!wait_for_devices) {
      continue;
    }
    dots = dots > 3 ? 0 : dots + 1;
    zigbee_screens_module_waiting_for_devices(dots);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

static void esp_zb_task(void* pvParameters) {
  /* initialize Zigbee stack */
  esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
  esp_zb_init(&zb_nwk_cfg);
  esp_zb_on_off_switch_cfg_t switch_cfg = ESP_ZB_DEFAULT_ON_OFF_SWITCH_CONFIG();
  esp_zb_ep_list_t* esp_zb_on_off_switch_ep =
      esp_zb_on_off_switch_ep_create(HA_ONOFF_SWITCH_ENDPOINT, &switch_cfg);
  esp_zb_device_register(esp_zb_on_off_switch_ep);
  esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
  ESP_ERROR_CHECK(esp_zb_start(false));
  esp_zb_main_loop_iteration();
}

void zigbee_switch_state_machine(button_event_t button_pressed) {
  uint8_t button_name = button_pressed >> 4;  // >> 4 to get the button number
  uint8_t button_event =
      button_pressed & 0x0F;  // & 0x0F to get the event number without the mask

  switch (button_name) {
    case BUTTON_RIGHT:
      switch (button_event) {
        case BUTTON_PRESS_DOWN:
          if (light_found) {
            zigbee_screens_module_toogle_pressed();
          }
          break;
        case BUTTON_PRESS_UP:
          if (light_found) {
            zigbee_screens_module_toggle_released();
            zigbee_switch_toggle();
          }
          break;
      }
      break;
    case BUTTON_LEFT:
      switch (button_event) {
        case BUTTON_PRESS_DOWN:
          zigbee_switch_deinit();
          break;
      }
      break;
    default:
      ESP_LOGI(TAG, "Button: %s, Event: %s", button_names[button_name],
               button_events_table[button_event]);
      break;
  }
}

void zigbee_switch_init() {
  esp_zb_platform_config_t config = {
      .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
      .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
  };
  ESP_ERROR_CHECK(esp_zb_platform_config(&config));
  menu_screens_set_app_state(true, zigbee_switch_state_machine);
  xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
  xTaskCreate(network_failed_task, "Network_failed", 4096, NULL, 5,
              &network_failed_task_handle);
  xTaskCreate(wait_for_devices_task, "Wait_for_devices", 4096, NULL, 5,
              &wait_for_devices_task_handle);
}

void zigbee_switch_deinit() {
  zigbee_screens_module_closing_network();
  preferences_put_bool("zigbee_deinit", true);
  esp_zb_factory_reset();
}

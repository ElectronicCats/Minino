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
#include "string.h"

typedef struct light_bulb_device_params_s {
  esp_zb_ieee_addr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
} light_bulb_device_params_t;

typedef enum {
  SWITCH_CREATE_NETWORK = 0,
  SWITCH_NETWORK_FAILED,
  SWITCH_WAIT_FOR_DEVICES,
  SWITCH_NO_DEVICES,
  SWITCH_LIGHT_FOUND,
  SWITCH_EXIT,
} switch_state_t;

const char* switch_state_names[] = {
    "SWITCH_CREATE_NETWORK", "SWITCH_NETWORK_FAILED", "SWITCH_WAIT_FOR_DEVICES",
    "SWITCH_LIGHT_FOUND",    "SWITCH_NO_DEVICES",     "SWITCH_EXIT",
};

volatile switch_state_t switch_state = SWITCH_CREATE_NETWORK;
switch_state_t switch_state_prev = SWITCH_EXIT;
uint16_t open_network_duration = 0;

TaskHandle_t network_failed_task_handle = NULL;
TaskHandle_t wait_for_devices_task_handle = NULL;
TaskHandle_t switch_state_machine_task_handle = NULL;
TaskHandle_t network_open_task_handle = NULL;

display_status_cb_t zigbee_switch_display_status_cb = NULL;

static const char* TAG = "ESP_ZB_ON_OFF_SWITCH";

bool zigbee_switch_is_light_connected() {
  return switch_state == SWITCH_LIGHT_FOUND;
}

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
    switch_state = SWITCH_LIGHT_FOUND;
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
      switch_state = SWITCH_CREATE_NETWORK;
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
        switch_state = SWITCH_WAIT_FOR_DEVICES;
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
          ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds",
                   esp_zb_get_pan_id(),
                   *(uint8_t*) esp_zb_app_signal_get_params(p_sg_p));
          open_network_duration =
              *(uint8_t*) esp_zb_app_signal_get_params(p_sg_p);
          vTaskResume(network_open_task_handle);
        } else {
          if (switch_state != SWITCH_LIGHT_FOUND &&
              switch_state != SWITCH_WAIT_FOR_DEVICES) {
            switch_state = SWITCH_NETWORK_FAILED;
          }
          if (switch_state == SWITCH_WAIT_FOR_DEVICES) {
            switch_state = SWITCH_NO_DEVICES;
          }
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
 * @brief Task to keep track of the network open duration
 *
 * @param pvParameters
 *
 * @return void
 */
void network_open_task(void* pvParameters) {
  while (true) {
    open_network_duration =
        open_network_duration > 0 ? open_network_duration - 1 : 0;
    if (open_network_duration > 0 && open_network_duration <= 10) {
      ESP_LOGI(TAG, "Network open for %d seconds", open_network_duration);
    }
    if (open_network_duration > 0 && open_network_duration % 10 == 0) {
      ESP_LOGI(TAG, "Network open for %d seconds", open_network_duration);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
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
    if (switch_state == SWITCH_NETWORK_FAILED) {
      // Wait for 2 seconds to check if the network creation failed
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      if (switch_state == SWITCH_NETWORK_FAILED) {
        zigbee_switch_display_status_cb(CREATING_NETWORK_FAILED);
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
  while (true) {
    if (switch_state != SWITCH_WAIT_FOR_DEVICES) {
      continue;
    }
    zigbee_switch_display_status_cb(WAITING_FOR_DEVICES);
    vTaskDelay(300 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Task to handle the switch state machine
 *
 * @param pvParameters
 *
 * @return void
 */
void switch_state_machine_task(void* pvParameters) {
  while (true) {
    if (switch_state != switch_state_prev) {
      ESP_LOGI(TAG, "Switch state: %s", switch_state_names[switch_state]);

      switch (switch_state) {
        case SWITCH_CREATE_NETWORK:
          zigbee_switch_display_status_cb(CREATING_NETWORK);
          break;
        case SWITCH_WAIT_FOR_DEVICES:
          break;
        case SWITCH_NO_DEVICES:
          zigbee_switch_display_status_cb(NO_DEVICES_FOUND);
          break;
        case SWITCH_LIGHT_FOUND:
          zigbee_switch_display_status_cb(LIGHT_RELASED);
          break;
        default:
          break;
      }
      // zigbee_switch_display_status_cb(switch_state);
      switch_state_prev = switch_state;
    }
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

void zigbee_switch_init() {
  esp_zb_platform_config_t config = {
      .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
      .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
  };
  ESP_ERROR_CHECK(esp_zb_platform_config(&config));
  xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
  xTaskCreate(network_failed_task, "Network_failed", 4096, NULL, 5,
              &network_failed_task_handle);
  xTaskCreate(wait_for_devices_task, "Wait_for_devices", 4096, NULL, 5,
              &wait_for_devices_task_handle);
  xTaskCreate(switch_state_machine_task, "Switch_state_machine", 4096, NULL, 5,
              &switch_state_machine_task_handle);
  xTaskCreate(network_open_task, "Network_open", 4096, NULL, 5,
              &network_open_task_handle);
  vTaskSuspend(network_open_task_handle);
}

void zigbee_switch_deinit() {
  switch_state = SWITCH_EXIT;
  vTaskDelay(50 / portTICK_PERIOD_MS);
  zigbee_switch_display_status_cb(CLOSING_NETWORK);
  esp_zb_factory_reset();
}

void zigbee_switch_set_display_status_cb(display_status_cb_t cb) {
  zigbee_switch_display_status_cb = cb;
}
void zigbee_switch_remove_display_status_cb(display_status_cb_t cb) {
  zigbee_switch_display_status_cb = NULL;
}

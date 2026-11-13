// SPDX-License-Identifier: GPL-3.0-or-later
#include "zigbee_mesh_mapper.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ieee_sniffer.h"
#include "menu_gfx.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "radio_selector.h"
#include "sd_card.h"
#include "wifi_service.h"

#define TAG            "zb_mesh_map"
#define MAX_MESH_LINKS 32
#define ZB_MESH_DIR    SD_CARD_PATH "/zigbee"
#define ZB_MESH_CSV    ZB_MESH_DIR "/mesh_topology.csv"
#define ZB_CSV_HEADER \
  "Timestamp,Channel,PAN_ID,Src_Short,Dst_Short,Frame_Type,LQI,RSSI,Packets\n"
#define ZB_PREF_CHANNEL_KEY "zb_map_chan"

typedef struct {
  uint16_t pan_id;
  uint16_t src_addr;
  uint16_t dst_addr;
  uint8_t lqi;
  int8_t rssi;
  uint32_t packets;
  char type_str[6];
} zb_mesh_link_t;

static bool s_running = false;
static uint8_t s_channel = 15;
static TaskHandle_t s_ui_task_handle = NULL;
static TaskHandle_t s_sniffer_task = NULL;
static SemaphoreHandle_t s_links_mutex = NULL;

static zb_mesh_link_t s_links[MAX_MESH_LINKS];
static uint16_t s_link_count = 0;
static uint16_t s_selected_idx = 0;
static uint32_t s_total_packets = 0;
static bool s_sd_logging = false;

static void parse_and_record_frame(uint8_t* packet, uint8_t packet_len) {
  if (packet == NULL || packet_len < 5) {
    return;
  }

  uint16_t fcf = packet[0] | ((uint16_t) packet[1] << 8);
  uint8_t f_type = fcf & 0x07;
  bool pan_compressed = (fcf & (1 << 6)) != 0;
  uint8_t dst_addr_mode = (fcf >> 10) & 0x03;
  uint8_t src_addr_mode = (fcf >> 14) & 0x03;

  const char* type_name = "DATA";
  if (f_type == 0x00)
    type_name = "BEAC";
  else if (f_type == 0x01)
    type_name = "DATA";
  else if (f_type == 0x02)
    type_name = "ACK";
  else if (f_type == 0x03)
    type_name = "CMD";

  uint8_t pos = 3;  // Skip FCF (2) + Sequence (1)
  uint16_t pan_id = 0xFFFF;
  uint16_t dst_addr = 0xFFFF;
  uint16_t src_addr = 0xFFFF;

  // Destination addressing
  if (dst_addr_mode == 2) {
    // Short 16-bit address
    if (pos + 4 > packet_len)
      return;
    pan_id = packet[pos] | ((uint16_t) packet[pos + 1] << 8);
    pos += 2;
    dst_addr = packet[pos] | ((uint16_t) packet[pos + 1] << 8);
    pos += 2;
  } else if (dst_addr_mode == 3) {
    // Extended 64-bit address
    if (pos + 10 > packet_len)
      return;
    pan_id = packet[pos] | ((uint16_t) packet[pos + 1] << 8);
    pos += 10;
  }

  // Source addressing
  if (src_addr_mode == 2) {
    if (!pan_compressed) {
      if (pos + 2 > packet_len)
        return;
      pan_id = packet[pos] | ((uint16_t) packet[pos + 1] << 8);
      pos += 2;
    }
    if (pos + 2 > packet_len)
      return;
    src_addr = packet[pos] | ((uint16_t) packet[pos + 1] << 8);
    pos += 2;
  }

  // If no source address found, skip
  if (src_addr == 0xFFFF && dst_addr == 0xFFFF) {
    return;
  }

  // The PSDU does not carry link metrics: read the values reported by the
  // radio for the frame that was just received. The last two bytes of an
  // 802.15.4 frame are the FCS, not RSSI/LQI.
  uint8_t lqi = ieee_sniffer_get_lqi();
  int8_t rssi = ieee_sniffer_get_rssi();
  if (rssi > 0 || rssi < -100) {
    rssi = -70;
  }

  s_total_packets++;

  if (s_links_mutex != NULL &&
      xSemaphoreTake(s_links_mutex, pdMS_TO_TICKS(30)) == pdTRUE) {
    int found_idx = -1;
    for (uint16_t i = 0; i < s_link_count; i++) {
      if (s_links[i].pan_id == pan_id && s_links[i].src_addr == src_addr &&
          s_links[i].dst_addr == dst_addr) {
        found_idx = i;
        break;
      }
    }

    if (found_idx >= 0) {
      s_links[found_idx].packets++;
      s_links[found_idx].lqi = lqi;
      s_links[found_idx].rssi = rssi;
      strncpy(s_links[found_idx].type_str, type_name,
              sizeof(s_links[found_idx].type_str) - 1);
    } else if (s_link_count < MAX_MESH_LINKS) {
      s_links[s_link_count].pan_id = pan_id;
      s_links[s_link_count].src_addr = src_addr;
      s_links[s_link_count].dst_addr = dst_addr;
      s_links[s_link_count].lqi = lqi;
      s_links[s_link_count].rssi = rssi;
      s_links[s_link_count].packets = 1;
      strncpy(s_links[s_link_count].type_str, type_name,
              sizeof(s_links[s_link_count].type_str) - 1);
      s_link_count++;
    }
    xSemaphoreGive(s_links_mutex);
  }

  // Log to SD card if active
  if (s_sd_logging) {
    char line[128];
    snprintf(line, sizeof(line), "%lu,%u,0x%04X,0x%04X,0x%04X,%s,%u,%d,%lu\n",
             (unsigned long) esp_log_timestamp(), s_channel, pan_id, src_addr,
             dst_addr, type_name, lqi, rssi, (unsigned long) s_total_packets);
    sd_card_append_to_file(ZB_MESH_CSV, line);
  }
}

static void render_screen(void) {
  char buf[32];
  oled_screen_clear_buffer();

  uint8_t pages = oled_screen_get_pages();

  uint16_t count = 0;
  uint16_t sel = 0;
  zb_mesh_link_t cur = {0};

  if (s_links_mutex != NULL &&
      xSemaphoreTake(s_links_mutex, pdMS_TO_TICKS(40)) == pdTRUE) {
    count = s_link_count;
    if (count > 0) {
      if (s_selected_idx >= count) {
        s_selected_idx = 0;
      }
      sel = s_selected_idx;
      cur = s_links[sel];
    }
    xSemaphoreGive(s_links_mutex);
  }

  if (pages >= 8) {
    // 128x64 display (PEK Spectrum style)
    // 1. Header (y=1..10): 16 chars max, 8px/char
    // Left: "ZIGBEE" (6 chars = 48px, x=2..49)
    menu_gfx_text(2, 1, "ZIGBEE", 1, true);

    // Center: "CH%02u" (4 chars = 32px, x=56..87)
    snprintf(buf, sizeof(buf), "C:%02u", s_channel);
    menu_gfx_text(56, 1, buf, 1, true);

    // Right: "[0]" or "1/3" (x=98..125)
    if (count > 0) {
      snprintf(buf, sizeof(buf), "%u/%u", sel + 1, count);
    } else {
      snprintf(buf, sizeof(buf), "[0]");
    }
    int nw = menu_gfx_text_width(buf, 1);
    menu_gfx_text(126 - nw, 1, buf, 1, true);

    oled_screen_draw_hline(0, 10, 128, OLED_DISPLAY_NORMAL);

    // 2. PAN & Frame Type (y=13..21): max 15 chars (120px)
    if (count > 0) {
      snprintf(buf, sizeof(buf), "PAN:%04X [%s]", cur.pan_id, cur.type_str);
    } else {
      snprintf(buf, sizeof(buf), "SEARCHING MESH");
    }
    int pw = menu_gfx_text_width(buf, 1);
    menu_gfx_text((128 - pw) / 2, 13, buf, 1, true);

    // 3. Source & Destination Addresses (y=23..39)
    if (count > 0) {
      snprintf(buf, sizeof(buf), "%04X -> %04X", cur.src_addr, cur.dst_addr);
      int aw = menu_gfx_text_width(buf, 1);
      menu_gfx_text((128 - aw) / 2, 23, buf, 1, true);

      snprintf(buf, sizeof(buf), "LQI:%u %ddBm", cur.lqi, cur.rssi);
      int lw = menu_gfx_text_width(buf, 1);
      menu_gfx_text((128 - lw) / 2, 33, buf, 1, true);
    } else {
      menu_gfx_text(8, 23, "NO NODES FOUND", 1, true);
      menu_gfx_text(16, 33, "CH HOP: ^ UP", 1, true);
    }

    // 4. LQI Signal Meter (y=43..48)
    const int bar_x = 10;
    const int bar_y = 43;
    const int bar_w = 108;
    const int bar_h = 6;
    oled_screen_draw_rect(bar_x, bar_y, bar_w, bar_h, OLED_DISPLAY_NORMAL);

    if (count > 0) {
      int fill_w = ((int) cur.lqi * (bar_w - 4)) / 255;
      if (fill_w > (bar_w - 4))
        fill_w = bar_w - 4;
      if (fill_w > 0) {
        oled_screen_draw_box(bar_x + 2, bar_y + 2, fill_w, bar_h - 4,
                             OLED_DISPLAY_NORMAL);
      }
    }

    // 5. Footer (y=53..61)
    menu_gfx_text(2, 53, "<BACK", 1, true);
    menu_gfx_text(52, 53, "^CH", 1, true);
    if (count > 1) {
      menu_gfx_text(86, 53, "NEXT>", 1, true);
    }

  } else {
    // 128x32 compact display
    snprintf(buf, sizeof(buf), "ZB MESH Ch:%02u [%u]", s_channel, count);
    menu_gfx_text_clip(2, 1, buf, 126, true);

    if (count > 0) {
      snprintf(buf, sizeof(buf), "%04X>%04X L:%u", cur.src_addr, cur.dst_addr,
               cur.lqi);
    } else {
      snprintf(buf, sizeof(buf), "Listening...");
    }
    menu_gfx_text(2, 12, buf, 1, true);

    const int bar_x = 8;
    const int bar_y = 24;
    const int bar_w = 112;
    const int bar_h = 5;
    oled_screen_draw_rect(bar_x, bar_y, bar_w, bar_h, OLED_DISPLAY_NORMAL);
    if (count > 0) {
      int fill_w = ((int) cur.lqi * (bar_w - 2)) / 255;
      if (fill_w > 0) {
        oled_screen_draw_box(bar_x + 1, bar_y + 1, fill_w, bar_h - 2,
                             OLED_DISPLAY_NORMAL);
      }
    }
  }

  oled_screen_display_show();
}

static void ui_update_task(void* arg) {
  (void) arg;
  while (s_running) {
    render_screen();
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  s_ui_task_handle = NULL;
  vTaskDelete(NULL);
}

static void mesh_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }

  switch (button_name) {
    case BUTTON_LEFT:
      zigbee_mesh_mapper_stop();
      break;
    case BUTTON_RIGHT:
    case BUTTON_DOWN:
      if (s_link_count > 0) {
        s_selected_idx = (s_selected_idx + 1) % s_link_count;
      }
      break;
    case BUTTON_UP:
      s_channel = (s_channel >= 26) ? 11 : (s_channel + 1);
      preferences_put_int(ZB_PREF_CHANNEL_KEY, s_channel);
      ieee_sniffer_set_channel(s_channel);
      break;
    default:
      break;
  }
}

void zigbee_mesh_mapper_begin(void) {
  if (s_running) {
    return;
  }

  s_running = true;
  s_link_count = 0;
  s_selected_idx = 0;
  s_total_packets = 0;
  s_sd_logging = false;

  if (s_links_mutex == NULL) {
    s_links_mutex = xSemaphoreCreateMutex();
  }

  s_channel = (uint8_t) preferences_get_int(ZB_PREF_CHANNEL_KEY, 15);
  if (s_channel < 11 || s_channel > 26) {
    s_channel = 15;
  }

  // Release Wi-Fi to free radio for 802.15.4
  wifi_service_deinit();
  radio_selector_set_zigbee_sniffer();

  menus_module_set_app_state(true, mesh_input_cb);

  // Mount SD card and setup CSV log
  if (sd_card_mount() == ESP_OK) {
    if (sd_card_create_dir(ZB_MESH_DIR) == ESP_OK) {
      struct stat st;
      if (stat(ZB_MESH_CSV, &st) != 0 || st.st_size == 0) {
        sd_card_append_to_file(ZB_MESH_CSV, (char*) ZB_CSV_HEADER);
      }
      s_sd_logging = true;
      ESP_LOGI(TAG, "Logging Zigbee mesh nodes to %s", ZB_MESH_CSV);
    }
  }

  // Initialize IEEE 802.15.4 sniffer
  ieee_sniffer_register_cb(parse_and_record_frame);
  ieee_sniffer_set_channel(s_channel);

  xTaskCreate(ieee_sniffer_begin, "zb_mesh_sniff", 3072, NULL, 5,
              &s_sniffer_task);
  xTaskCreate(ui_update_task, "zb_mesh_ui", 3072, NULL, 4, &s_ui_task_handle);
}

void zigbee_mesh_mapper_stop(void) {
  if (!s_running) {
    return;
  }

  s_running = false;
  ieee_sniffer_stop();
  ieee_sniffer_register_cb(NULL);

  for (int i = 0; i < 100 && s_ui_task_handle != NULL; i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  vTaskDelay(pdMS_TO_TICKS(30));
  s_sniffer_task = NULL;
  s_ui_task_handle = NULL;

  wifi_service_init(WIFI_MODE_STA);
  menus_module_set_default_input();
  oled_screen_clear();
  menus_module_exit_app();
}

void zigbee_mesh_mapper_on_exit(void) {
  if (!s_running) {
    return;
  }
  s_running = false;
  ieee_sniffer_stop();
  ieee_sniffer_register_cb(NULL);
  for (int i = 0; i < 100 && s_ui_task_handle != NULL; i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  vTaskDelay(pdMS_TO_TICKS(30));
  s_sniffer_task = NULL;
  s_ui_task_handle = NULL;
  wifi_service_init(WIFI_MODE_STA);
}

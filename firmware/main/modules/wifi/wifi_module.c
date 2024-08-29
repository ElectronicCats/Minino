#include "wifi_module.h"

#include "captive_portal.h"
#include "catdos_module.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "keyboard_module.h"
#include "string.h"

#include "deauth_module.h"
#include "general_radio_selection.h"
#include "general_screens.h"
#include "led_events.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "sd_card.h"
#include "wifi_attacks.h"
#include "wifi_controller.h"
#include "wifi_module.h"
#include "wifi_scanner.h"
#include "wifi_screens_module.h"

static const char* TAG = "wifi_module";
bool analizer_initialized = false;

static general_menu_t analyzer_summary_menu;
static char* wifi_analizer_summary_2[120] = {
    "Summary",
};
static const char* wifi_analizer_help_2[] = {
    "This tool",      "allows you to",   "analyze the",
    "WiFi networks",  "around you.",     "",
    "You can select", "the channel and", "the destination",
    "to save the",    "results.",
};

static const general_menu_t analyzer_help_menu = {
    .menu_items = wifi_analizer_help_2,
    .menu_count = 11,
    .menu_level = GENERAL_TREE_APP_MENU};

static const char* destination_options[] = {"SD", "Internal"};
static const char* channel_options[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
};

void wifi_module_show_analyzer_help() {
  general_register_scrolling_menu(&analyzer_help_menu);
  general_screen_display_scrolling_text_handler(menus_module_exit_app);
}
static void wifi_module_input_cb(uint8_t button_name, uint8_t button_event);

uint16_t get_summary_rows_count() {
  uint8_t num_items = 0;
  char** submenu = wifi_analizer_summary_2;
  if (submenu != NULL) {
    while (submenu[num_items] != NULL) {
      num_items++;
    }
  }

  if (num_items == 0) {
    return -1;
  }
  return num_items;
}

void wifi_module_init_sniffer() {
  oled_screen_clear();
  if (wifi_sniffer_is_destination_sd()) {
    esp_err_t err = sd_card_mount();
    switch (err) {
      case ESP_OK:
        ESP_LOGI(TAG, "SD card mounted");
        break;
      case ESP_ERR_NOT_SUPPORTED:
        ESP_LOGI(TAG, "SD card not supported");
        wifi_screeens_show_sd_not_supported();
        wifi_sniffer_set_destination_internal();
        // TODO: add an option to format the SD card
        break;
      default:
        ESP_LOGE(TAG, "SD card mount failed: reason: %s", esp_err_to_name(err));
      case ESP_ERR_NOT_FOUND:
        ESP_LOGW(TAG, "SD card not found");
        wifi_screeens_show_sd_not_found();
        wifi_sniffer_set_destination_internal();
        break;
    }
  }
  wifi_sniffer_start();
  led_control_run_effect(led_control_zigbee_scanning);
}
static void wifi_module_summary_exit_cb() {
  wifi_sniffer_close_file();
  menus_module_exit_app();
}

void wifi_module_analyzer_run_exit() {
  analyzer_summary_menu.menu_items = wifi_analizer_summary_2;
  analyzer_summary_menu.menu_level = GENERAL_TREE_APP_MENU;
  wifi_sniffer_stop();
  led_control_stop();
  wifi_sniffer_load_summary();
  analyzer_summary_menu.menu_count = get_summary_rows_count();
  general_register_scrolling_menu(&analyzer_summary_menu);
  general_screen_display_scrolling_text_handler(wifi_module_summary_exit_cb);
}

void wifi_module_analyzer_summary_exit() {
  wifi_sniffer_close_file();
}

void wifi_module_analyzer_exit() {
  menus_module_restart();
}

void wifi_module_analyzer_destination_exit() {
  if (wifi_sniffer_is_destination_sd()) {
    // Verify if the SD card is inserted
    sd_card_unmount();
    if (sd_card_mount() == ESP_OK) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      sd_card_unmount();
    } else {
      wifi_sniffer_set_destination_internal();
    }
  }
}

void wifi_module_analyzer_run() {
  wifi_module_init_sniffer();
  menus_module_set_app_state(true, wifi_module_input_cb);
}

static void wifi_module_set_destination(uint8_t selected_item) {
  if (selected_item == WIFI_SNIFFER_DESTINATION_SD) {
    wifi_sniffer_set_destination_sd();
  } else {
    wifi_sniffer_set_destination_internal();
  }
}
static void wifi_module_set_channel(uint8_t selected_item) {
  wifi_sniffer_set_channel(selected_item + 1);
}

void wifi_module_analyzer_channel() {
  general_radio_selection_menu_t channel = {0};
  channel.banner = "Choose Channel",
  channel.current_option = wifi_sniffer_get_channel() - 1;
  channel.options = channel_options;
  channel.options_count = 14;
  channel.select_cb = wifi_module_set_channel;
  channel.exit_cb = menus_module_exit_app;
  channel.style = RADIO_SELECTION_OLD_STYLE;
  general_radio_selection(channel);
}

void wifi_module_analyzer_destination() {
  general_radio_selection_menu_t destination = {0};
  destination.banner = "Choose Destination",
  destination.current_option = wifi_sniffer_is_destination_internal();
  destination.options = destination_options;
  destination.options_count = 2;
  destination.select_cb = wifi_module_set_destination;
  destination.exit_cb = menus_module_exit_app;
  destination.style = RADIO_SELECTION_OLD_STYLE;
  general_radio_selection(destination);
}
void wifi_module_analizer_begin() {
  ESP_LOGI(TAG, "Initializing WiFi analizer module");
  wifi_sniffer_register_cb(wifi_screens_module_display_sniffer_cb);
  wifi_sniffer_register_animation_cbs(wifi_screens_sniffer_animation_start,
                                      wifi_screens_sniffer_animation_stop);
  wifi_sniffer_register_summary_cb(wifi_module_analizer_summary_cb);
  wifi_sniffer_begin();
  analizer_initialized = true;
}

void wifi_module_analizer_summary_cb(FILE* pcap_file) {
  esp_err_t ret = ESP_OK;
  long size = pcap_cmd_get_file_size(pcap_file);
  char* packet_payload = NULL;
  // packet index (by bytes)
  uint32_t index = 0;
  pcap_file_header_t file_header;
  size_t real_read =
      fread(&file_header, sizeof(pcap_file_header_t), 1, pcap_file);
  if (real_read != 1) {
    ESP_LOGE(TAG, "read pcap file header failed");
    return;
  }
  index += sizeof(pcap_file_header_t);

  // Get the header information
  char* magic_number_str = malloc(16);
  snprintf(magic_number_str, 16, " %" PRIx32, file_header.magic);
  char* major_version_str = malloc(21);
  sprintf(major_version_str, "Major Version: %d", file_header.major);
  char* snaplen_str = malloc(21);
  snprintf(snaplen_str, 16, "SnapLen: %" PRIu32, file_header.snaplen);
  char* link_type_str = malloc(21);
  snprintf(link_type_str, 16, "LinkType: %" PRIu32, file_header.link_type);

  // Load header information
  uint32_t summary_index = 1;  // Skip scroll text flag and Summary title
  wifi_analizer_summary_2[summary_index++] = "----------------";
  wifi_analizer_summary_2[summary_index++] = "Magic Number:";
  wifi_analizer_summary_2[summary_index++] = magic_number_str;
  wifi_analizer_summary_2[summary_index++] = major_version_str;
  wifi_analizer_summary_2[summary_index++] = snaplen_str;
  wifi_analizer_summary_2[summary_index++] = link_type_str;
  wifi_analizer_summary_2[summary_index++] = "----------------";

  uint32_t packet_num = 0;
  pcap_packet_header_t packet_header;
  while (index < size) {
    if (packet_num == 4) {
      break;
    }
    real_read =
        fread(&packet_header, sizeof(pcap_packet_header_t), 1, pcap_file);
    ESP_GOTO_ON_FALSE(real_read == 1, ESP_FAIL, err, TAG,
                      "read pcap packet header failed");

    // Get the packet header information
    char* packet_num_str = malloc(22);
    snprintf(packet_num_str, 21, "Packet %" PRIu32 ":", packet_num);
    char* timestamp_seconds_str = malloc(32);
    snprintf(timestamp_seconds_str, 32, "Timestamp: %" PRIu32 "s",
             packet_header.seconds);
    char* capture_length_str = malloc(32);
    snprintf(capture_length_str, 32, "Capture Len: %" PRIu32,
             packet_header.capture_length);
    char* packet_length_str = malloc(32);
    snprintf(packet_length_str, 32, "Packet Len: %" PRIu32,
             packet_header.packet_length);

    // Load packet header information
    wifi_analizer_summary_2[summary_index++] = packet_num_str;
    wifi_analizer_summary_2[summary_index++] = timestamp_seconds_str;
    wifi_analizer_summary_2[summary_index++] = capture_length_str;
    wifi_analizer_summary_2[summary_index++] = packet_length_str;

    size_t payload_length = packet_header.capture_length;
    packet_payload = malloc(payload_length);
    ESP_GOTO_ON_FALSE(packet_payload, ESP_ERR_NO_MEM, err, TAG,
                      "no mem to save packet payload");
    real_read = fread(packet_payload, payload_length, 1, pcap_file);
    ESP_GOTO_ON_FALSE(real_read == 1, ESP_FAIL, err, TAG, "read payload error");
    if (file_header.link_type == PCAP_LINK_TYPE_802_11) {
      // Check if the frame is a Beacon frame or Probe Response frame
      uint8_t frame_type = (packet_payload[0] >> 2) & 0x03;
      uint8_t frame_subtype = (packet_payload[0] >> 4) & 0x0F;
      if ((frame_type == 0 && frame_subtype == 8) ||  // Beacon frame
          (frame_type == 0 && frame_subtype == 5)) {  // Probe Response frame
        uint8_t ssid_length = packet_payload[37];
        uint8_t supported_rates_length = packet_payload[38 + ssid_length + 1];
        // The SSID parameter set is located after the fixed parameters (36
        // bytes)
        char* ssid_str = malloc(32);
        snprintf(ssid_str, 32, "%s", &packet_payload[38]);
        char* channel_str = malloc(32);
        snprintf(channel_str, 32, "Channel: %d",
                 packet_payload[38 + ssid_length + supported_rates_length + 4]);
        // The BSSID is located in the Address 3 field
        char* bssid_str = malloc(32);
        char* bssid_str2 = malloc(32);
        snprintf(bssid_str, 32, "BSSID: %2X:%2X:%2X", packet_payload[16],
                 packet_payload[17], packet_payload[18]);
        snprintf(bssid_str2, 32, "       %2X:%2X:%2X", packet_payload[19],
                 packet_payload[20], packet_payload[21]);

        wifi_analizer_summary_2[summary_index++] = "SSID:";
        wifi_analizer_summary_2[summary_index++] = ssid_str;
        wifi_analizer_summary_2[summary_index++] = channel_str;
        wifi_analizer_summary_2[summary_index++] = bssid_str;
        wifi_analizer_summary_2[summary_index++] = bssid_str2;
      }
      // Frame Control Field is coded as LSB first
      char* frame_type_str = malloc(32);
      snprintf(frame_type_str, 32, "Frame Type:%2x",
               (packet_payload[0] >> 2) & 0x03);
      char* frame_subtype_str = malloc(32);
      snprintf(frame_subtype_str, 32, "Frame Subtype:%2x",
               (packet_payload[0] >> 4) & 0x0F);
      char* destination_str = malloc(32);
      char* destination_str2 = malloc(32);
      snprintf(destination_str, 32, "        %2X:%2X:%2X", packet_payload[4],
               packet_payload[5], packet_payload[6]);
      snprintf(destination_str2, 32, "        %2X:%2X:%2X", packet_payload[7],
               packet_payload[8], packet_payload[9]);
      char* source_str = malloc(32);
      char* source_str2 = malloc(32);
      snprintf(source_str, 32, "Source: %2X:%2X:%2X", packet_payload[10],
               packet_payload[11], packet_payload[12]);
      snprintf(source_str2, 32, "        %2X:%2X:%2X", packet_payload[13],
               packet_payload[14], packet_payload[15]);

      wifi_analizer_summary_2[summary_index++] = frame_type_str;
      wifi_analizer_summary_2[summary_index++] = frame_subtype_str;
      wifi_analizer_summary_2[summary_index++] = "Destination:";
      wifi_analizer_summary_2[summary_index++] = destination_str;
      wifi_analizer_summary_2[summary_index++] = destination_str2;
      wifi_analizer_summary_2[summary_index++] = source_str;
      wifi_analizer_summary_2[summary_index++] = source_str2;

      wifi_analizer_summary_2[summary_index++] = "----------------";
    } else {
      char* link_type_str = malloc(32);
      snprintf(link_type_str, 32, "Link Type: %" PRIu32, file_header.link_type);
      wifi_analizer_summary_2[summary_index++] = "Unknown link type";
      wifi_analizer_summary_2[summary_index++] = link_type_str;
    }
    free(packet_payload);
    packet_payload = NULL;
    index += packet_header.capture_length + sizeof(pcap_packet_header_t);
    packet_num++;
  }

  if (packet_num > 0) {
    wifi_analizer_summary_2[summary_index++] = "Open the pcap";
    wifi_analizer_summary_2[summary_index++] = "file in";
    wifi_analizer_summary_2[summary_index++] = "Wireshark to see";
    wifi_analizer_summary_2[summary_index++] = "more.";
  } else {
    wifi_analizer_summary_2[summary_index++] = "No packets found";
  }

  wifi_analizer_summary_2[summary_index++] = NULL;
  if (packet_payload) {
    free(packet_payload);
  }
  return;
err:
  if (packet_payload) {
    free(packet_payload);
  }
  wifi_analizer_summary_2[summary_index++] = NULL;
}

static void wifi_module_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      wifi_module_analyzer_run_exit();
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_UP:
      break;
    case BUTTON_DOWN:
      break;
    default:
      break;
  }
}

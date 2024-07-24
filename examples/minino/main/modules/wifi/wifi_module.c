
#include "modules/wifi/wifi_module.h"
#include "captive_portal.h"
#include "catdos_module.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "keyboard_module.h"
#include "string.h"

#include "captive_portal.h"
#include "led_events.h"
#include "menu_screens_modules.h"
#include "modules/wifi/wifi_module.h"
#include "modules/wifi/wifi_screens_module.h"
#include "oled_screen.h"
#include "sd_card.h"
#include "wifi_attacks.h"
#include "wifi_controller.h"
#include "wifi_scanner.h"

static const char* TAG = "wifi_module";
bool analizer_initialized = false;
const uint32_t SOUND_DURATION = 100;

/**
 * @brief Enum with the wifi module states
 *
 */
typedef enum {
  WIFI_STATE_SCANNING = 0,
  WIFI_STATE_SCANNED,
  WIFI_STATE_DETAILS,
  WIFI_STATE_ATTACK_SELECTOR,
  WIFI_STATE_ATTACK,
  WIFI_STATE_ATTACK_CAPTIVE_PORTAL,
} wifi_state_t;

/**
 * @brief Structure to store the wifi module data
 *
 */
typedef struct {
  wifi_state_t state;
  wifi_config_t wifi_config;
} wifi_module_t;

char* wifi_state_names[] = {
    "WIFI_STATE_SCANNING", "WIFI_STATE_SCANNED",
    "WIFI_STATE_DETAILS",  "WIFI_STATE_ATTACK_SELECTOR",
    "WIFI_STATE_ATTACK",   "WIFI_STATE_ATTACK_CAPTIVE_PORTAL"};

static TaskHandle_t task_display_scanning = NULL;
static TaskHandle_t task_display_attacking = NULL;
static wifi_scanner_ap_records_t* ap_records;
static wifi_module_t current_wifi_state;
static int current_option = 0;
static bool show_details = false;
static bool valid_records = false;
static int index_targeted = 0;

static void scanning_task(void* pvParameters) {
  while (!valid_records) {
    wifi_scanner_module_scan();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
  vTaskSuspend(task_display_scanning);
  wifi_screens_module_display_scanned_networks(
      ap_records->records, ap_records->count, current_option);
  vTaskDelete(NULL);
}

void wifi_module_init_sniffer() {
  oled_screen_clear();
  if (wifi_sniffer_is_destination_sd()) {
    esp_err_t err = sd_card_mount();
    switch (err) {
      case ESP_OK:
        ESP_LOGI(TAG, "SD card mounted");
        break;
      case ESP_ERR_ALREADY_MOUNTED:
        ESP_LOGI(TAG, "SD card already mounted");
        break;
      case ESP_ERR_NOT_SUPPORTED:
        ESP_LOGI(TAG, "SD card not supported");
        oled_screen_display_text_center("SD card not", 0, OLED_DISPLAY_NORMAL);
        oled_screen_display_text_center("supported", 1, OLED_DISPLAY_NORMAL);
        oled_screen_display_text_center("Switching to", 3, OLED_DISPLAY_NORMAL);
        oled_screen_display_text_center("internal storage", 4,
                                        OLED_DISPLAY_NORMAL);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        oled_screen_clear();
        wifi_sniffer_set_destination_internal();
        // TODO: add an option to format the SD card
        break;
      default:
        ESP_LOGE(TAG, "SD card mount failed: reason: %s", esp_err_to_name(err));
      case ESP_ERR_NOT_FOUND:
        ESP_LOGW(TAG, "SD card not found");
        oled_screen_display_text_center("SD card ", 0, OLED_DISPLAY_NORMAL);
        oled_screen_display_text_center("not found", 1, OLED_DISPLAY_NORMAL);
        oled_screen_display_text_center("Switching to", 3, OLED_DISPLAY_NORMAL);
        oled_screen_display_text_center("internal storage", 4,
                                        OLED_DISPLAY_NORMAL);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        oled_screen_clear();
        wifi_sniffer_set_destination_internal();
        break;
    }
  }

  wifi_sniffer_start();
  led_control_run_effect(led_control_zigbee_scanning);
}

void wifi_module_exit_submenu_cb() {
  screen_module_menu_t current_menu = menu_screens_get_current_menu();

  switch (current_menu) {
    case MENU_WIFI_APPS:
      menu_screens_unregister_submenu_cbs();
      break;
    case MENU_WIFI_ANALIZER_RUN:
      wifi_sniffer_stop();
      led_control_stop();
      break;
    case MENU_WIFI_ANALIZER_ASK_SUMMARY:
      oled_screen_clear();
      wifi_sniffer_start();
      led_control_run_effect(led_control_zigbee_scanning);
      break;
    case MENU_WIFI_ANALIZER_SUMMARY:
      wifi_sniffer_close_file();
      break;
    case MENU_WIFI_ANALIZER:
      screen_module_set_screen(MENU_WIFI_ANALIZER);
      esp_restart();
      break;
    case MENU_WIFI_ANALIZER_DESTINATION:
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
      break;
      // case MENU_WIFI_DOS:
      //   screen_module_set_screen(MENU_WIFI_DOS);
      //   esp_restart();
      break;
    default:
      break;
  }
}

void wifi_module_enter_submenu_cb(screen_module_menu_t user_selection) {
  uint8_t selected_item = menu_screens_get_selected_item();

  switch (user_selection) {
    case MENU_WIFI_ANALIZER:
      wifi_module_analizer_begin();
      break;
    case MENU_WIFI_DEAUTH:
      wifi_module_deauth_begin();
      break;
    case MENU_WIFI_DOS:
      oled_screen_clear();
      catdos_module_begin();
      break;
    case MENU_WIFI_ANALIZER_RUN:
      wifi_module_init_sniffer();
      break;
    case MENU_WIFI_ANALIZER_SUMMARY:
      wifi_sniffer_load_summary();
      break;
    case MENU_WIFI_ANALIZER_CHANNEL:
      if (menu_screens_is_configuration(user_selection)) {
        buzzer_play_for(SOUND_DURATION);
        wifi_sniffer_set_channel(selected_item + 1);
      }
      wifi_module_update_channel_options();
      break;
    case MENU_WIFI_ANALIZER_DESTINATION:
      if (menu_screens_is_configuration(user_selection)) {
        buzzer_play_for(SOUND_DURATION);
        if (selected_item == WIFI_SNIFFER_DESTINATION_SD) {
          wifi_sniffer_set_destination_sd();
        } else {
          wifi_sniffer_set_destination_internal();
        }
      }
      wifi_module_update_destination_options();
      break;
    default:
      break;
  }
}

void wifi_module_begin() {
#if !defined(CONFIG_WIFI_MODULE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  menu_screens_register_enter_submenu_cb(wifi_module_enter_submenu_cb);
  menu_screens_register_exit_submenu_cb(wifi_module_exit_submenu_cb);
}

void wifi_module_exit() {
  screen_module_set_screen(MENU_WIFI_DEAUTH);
  esp_restart();
}

void wifi_module_deauth_begin() {
  ESP_LOGI(TAG, "Initializing WiFi module");
  menu_screens_set_app_state(true, wifi_module_keyboard_cb);
  current_wifi_state.state = WIFI_STATE_SCANNING;
  memset(&current_wifi_state.wifi_config, 0, sizeof(wifi_config_t));
  current_wifi_state.wifi_config = wifi_driver_access_point_begin();

  xTaskCreate(wifi_screens_module_scanning, "wifi_module_scanning", 4096, NULL,
              5, &task_display_scanning);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  xTaskCreate(scanning_task, "wifi_module_scan", 4096, NULL, 5, NULL);
  ap_records = wifi_scanner_get_ap_records();

  while (!valid_records) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    if (ap_records->count > 10) {
      valid_records = true;
    }
    ap_records = wifi_scanner_get_ap_records();
  }

  current_wifi_state.state = WIFI_STATE_SCANNED;
}

void wifi_module_analizer_begin() {
  if (analizer_initialized) {
    ESP_LOGW(TAG, "WiFi analizer already initialized");
    return;
  }

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
  uint32_t summary_index = 2;  // Skip scroll text flag and Summary title
  wifi_analizer_summary[summary_index++] = "----------------";
  wifi_analizer_summary[summary_index++] = "Magic Number:";
  wifi_analizer_summary[summary_index++] = magic_number_str;
  wifi_analizer_summary[summary_index++] = major_version_str;
  wifi_analizer_summary[summary_index++] = snaplen_str;
  wifi_analizer_summary[summary_index++] = link_type_str;
  wifi_analizer_summary[summary_index++] = "----------------";

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
    wifi_analizer_summary[summary_index++] = packet_num_str;
    wifi_analizer_summary[summary_index++] = timestamp_seconds_str;
    wifi_analizer_summary[summary_index++] = capture_length_str;
    wifi_analizer_summary[summary_index++] = packet_length_str;

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

        wifi_analizer_summary[summary_index++] = "SSID:";
        wifi_analizer_summary[summary_index++] = ssid_str;
        wifi_analizer_summary[summary_index++] = channel_str;
        wifi_analizer_summary[summary_index++] = bssid_str;
        wifi_analizer_summary[summary_index++] = bssid_str2;
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

      wifi_analizer_summary[summary_index++] = frame_type_str;
      wifi_analizer_summary[summary_index++] = frame_subtype_str;
      wifi_analizer_summary[summary_index++] = "Destination:";
      wifi_analizer_summary[summary_index++] = destination_str;
      wifi_analizer_summary[summary_index++] = destination_str2;
      wifi_analizer_summary[summary_index++] = source_str;
      wifi_analizer_summary[summary_index++] = source_str2;

      wifi_analizer_summary[summary_index++] = "----------------";
    } else {
      char* link_type_str = malloc(32);
      snprintf(link_type_str, 32, "Link Type: %" PRIu32, file_header.link_type);
      wifi_analizer_summary[summary_index++] = "Unknown link type";
      wifi_analizer_summary[summary_index++] = link_type_str;
    }
    free(packet_payload);
    packet_payload = NULL;
    index += packet_header.capture_length + sizeof(pcap_packet_header_t);
    packet_num++;
  }

  if (packet_num > 0) {
    wifi_analizer_summary[summary_index++] = "Open the pcap";
    wifi_analizer_summary[summary_index++] = "file in";
    wifi_analizer_summary[summary_index++] = "Wireshark to see";
    wifi_analizer_summary[summary_index++] = "more.";
  } else {
    wifi_analizer_summary[summary_index++] = "No packets found";
  }

  wifi_analizer_summary[summary_index++] = NULL;
  if (packet_payload) {
    free(packet_payload);
  }
  return;
err:
  if (packet_payload) {
    free(packet_payload);
  }
  wifi_analizer_summary[summary_index++] = NULL;
}

void wifi_module_update_channel_options() {
  uint8_t selected_option = wifi_sniffer_get_channel();
  selected_option--;
  menu_screens_update_options(wifi_analizer_channel_items, selected_option);
}

void wifi_module_update_destination_options() {
  uint8_t selected_option = 0;
  if (wifi_sniffer_is_destination_internal()) {
    selected_option = 1;
  }
  menu_screens_update_options(wifi_analizer_destination_items, selected_option);
}

void wifi_module_keyboard_cb(button_event_t button_pressed) {
  uint8_t button_name = button_pressed >> 4;
  uint8_t button_event = button_pressed & 0x0F;
  ESP_LOGI(TAG, "State: %s", wifi_state_names[current_wifi_state.state]);

  switch (current_wifi_state.state) {
    case WIFI_STATE_SCANNING: {
      switch (button_name) {
        case BUTTON_LEFT: {
          if (button_event == BUTTON_DOUBLE_CLICK) {
            wifi_module_exit();
            break;
          }
          break;
        }
        case BUTTON_RIGHT:
        case BUTTON_UP:
        case BUTTON_DOWN:
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    }
    case WIFI_STATE_SCANNED: {
      switch (button_name) {
        case BUTTON_LEFT: {
          if (button_event == BUTTON_DOUBLE_CLICK) {
            wifi_module_exit();
            break;
          }
          break;
        }
        case BUTTON_RIGHT:
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          show_details = true;
          index_targeted = current_option;
          current_wifi_state.state = WIFI_STATE_DETAILS;

          current_option = 0;
          wifi_screens_module_display_details_network(
              &ap_records->records[index_targeted], current_option);
          break;
        case BUTTON_UP: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_option = (current_option == 0) ? 0 : current_option - 1;
          wifi_screens_module_display_scanned_networks(
              ap_records->records, ap_records->count, current_option);
          break;
        }
        case BUTTON_DOWN: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_option = (current_option == (ap_records->count - 1))
                               ? current_option
                               : current_option + 1;
          wifi_screens_module_display_scanned_networks(
              ap_records->records, ap_records->count, current_option);
          break;
        }
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    }
    case WIFI_STATE_DETAILS: {
      switch (button_name) {
        case BUTTON_LEFT: {
          if (button_event == BUTTON_DOUBLE_CLICK) {
            wifi_module_exit();
            break;
          }
          current_option = 0;
          show_details = false;
          current_wifi_state.state = WIFI_STATE_SCANNED;
          wifi_screens_module_display_scanned_networks(
              ap_records->records, ap_records->count, current_option);
          break;
        }
        case BUTTON_RIGHT:
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_wifi_state.state = WIFI_STATE_ATTACK_SELECTOR;
          int count_attacks = wifi_attacks_get_attack_count();
          wifi_screens_module_display_attack_selector(
              WIFI_ATTACKS_LIST, count_attacks, current_option);
          break;
        case BUTTON_UP: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_option = 0;
          wifi_screens_module_display_details_network(
              &ap_records->records[index_targeted], current_option);
          break;
        }
        case BUTTON_DOWN: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_option = 1;
          wifi_screens_module_display_details_network(
              &ap_records->records[index_targeted], current_option);
          break;
        }
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    }
    case WIFI_STATE_ATTACK_SELECTOR: {
      switch (button_name) {
        case BUTTON_LEFT: {
          if (button_event == BUTTON_DOUBLE_CLICK) {
            wifi_module_exit();
            break;
          }
          show_details = false;
          current_option = 0;
          current_wifi_state.state = WIFI_STATE_SCANNED;
          wifi_screens_module_display_scanned_networks(
              ap_records->records, ap_records->count, current_option);
          break;
        }
        case BUTTON_RIGHT:
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_wifi_state.state = WIFI_STATE_ATTACK;

          if (current_option == WIFI_ATTACK_MULTI_AP) {
            for (int i = 0; i < ap_records->count; i++) {
              wifi_attack_handle_attacks(WIFI_ATTACK_COMBINE,
                                         &ap_records->records[i]);
            }
          } else if (current_option == WIFI_ATTACK_PASSWORD) {
            current_wifi_state.state = WIFI_STATE_ATTACK_CAPTIVE_PORTAL;
            current_option = 0;
            wifi_screens_module_display_captive_selector(CAPTIVE_PORTALS_LIST,
                                                         2, current_option);
          } else {
            wifi_attack_handle_attacks(current_option,
                                       &ap_records->records[index_targeted]);

            xTaskCreate(wifi_screens_module_animate_attacking,
                        "wifi_module_scanning", 4096,
                        &ap_records->records[index_targeted], 5,
                        &task_display_attacking);
          }
          break;
        case BUTTON_UP: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          int count_attacks = wifi_attacks_get_attack_count();
          current_option = (current_option == 0) ? 0 : current_option - 1;
          wifi_screens_module_display_attack_selector(
              WIFI_ATTACKS_LIST, count_attacks, current_option);
          break;
        }
        case BUTTON_DOWN: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          int count_attacks = wifi_attacks_get_attack_count();
          current_option = (current_option == (count_attacks - 1))
                               ? current_option
                               : current_option + 1;
          wifi_screens_module_display_attack_selector(
              WIFI_ATTACKS_LIST, count_attacks, current_option);
          break;
        }
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    }
    case WIFI_STATE_ATTACK: {
      switch (button_name) {
        case BUTTON_LEFT: {
          if (button_event == BUTTON_DOUBLE_CLICK) {
            wifi_attacks_module_stop();
            wifi_module_exit();
            vTaskSuspend(task_display_attacking);
            break;
          }
          show_details = false;
          current_option = 0;
          wifi_attacks_module_stop();
          vTaskSuspend(task_display_attacking);

          current_wifi_state.state = WIFI_STATE_ATTACK_SELECTOR;
          int count_attacks = wifi_attacks_get_attack_count();
          wifi_screens_module_display_attack_selector(
              WIFI_ATTACKS_LIST, count_attacks, current_option);
          break;
        }
        case BUTTON_RIGHT:
          break;
        case BUTTON_UP: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_option = (current_option == 0) ? 0 : current_option - 1;
          break;
        }
        case BUTTON_DOWN: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_option =
              (current_option == (3 - 1)) ? current_option : current_option + 1;
          break;
        }
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    }
    case WIFI_STATE_ATTACK_CAPTIVE_PORTAL: {
      switch (button_name) {
        case BUTTON_LEFT: {
          current_wifi_state.state = WIFI_STATE_ATTACK_SELECTOR;
          int count_attacks = wifi_attacks_get_attack_count();
          wifi_screens_module_display_attack_selector(
              WIFI_ATTACKS_LIST, count_attacks, current_option);
        }
        case BUTTON_RIGHT:
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          char* wifi_ssid = malloc(
              strlen((char*) ap_records->records[index_targeted].ssid) + 2);
          strcpy(wifi_ssid, (char*) ap_records->records[index_targeted].ssid);
          wifi_ssid[strlen((char*) ap_records->records[index_targeted].ssid)] =
              ' ';
          wifi_ssid[strlen((char*) ap_records->records[index_targeted].ssid) +
                    1] = '\0';

          wifi_config_t wifi_config_captive = {
              .ap = {.ssid = "",
                     .ssid_len = 0,
                     .password = "",
                     .max_connection = 4,
                     .authmode = WIFI_AUTH_WPA_WPA2_PSK}};
          strncpy((char*) wifi_config_captive.ap.ssid, wifi_ssid,
                  strlen(wifi_ssid));
          wifi_config_captive.ap.ssid[strlen(wifi_ssid)] = '\0';
          wifi_config_captive.ap.ssid_len = strlen(wifi_ssid);
          captive_portal_set_config(&wifi_config_captive);

          if (current_option == 0) {
            captive_portal_register_cb(
                wifi_screens_module_display_captive_user_pass);
            wifi_screens_module_display_captive_user_pass(
                (char*) ap_records->records[index_targeted].ssid, "", "");
          } else {
            captive_portal_register_cb(
                wifi_screens_module_display_captive_pass);
            wifi_screens_module_display_captive_pass(
                (char*) ap_records->records[index_targeted].ssid, "", "");
          }

          captive_portal_set_portal(current_option);

          xTaskCreate(captive_portal_begin, "captive_portal_start", 4096, NULL,
                      5, NULL);

          // wifi_attack_handle_attacks(WIFI_ATTACK_COMBINE,
          // &ap_records->records[index_targeted]);
          break;
        case BUTTON_UP: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_option = (current_option == 0) ? 0 : current_option - 1;
          wifi_screens_module_display_captive_selector(CAPTIVE_PORTALS_LIST, 2,
                                                       current_option);
          break;
        }
        case BUTTON_DOWN: {
          if (button_event != BUTTON_SINGLE_CLICK) {
            break;  // Only accept single click
          }
          current_option =
              (current_option == (3 - 1)) ? current_option : current_option + 1;

          wifi_screens_module_display_captive_selector(CAPTIVE_PORTALS_LIST, 2,
                                                       current_option);
          break;
        }
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
}

#include "gps_hw.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gps_module.h"
#include "preferences.h"

// GPS preference keys (moved from gps_screens.h to avoid circular dependency)
#define AGNSS_OPTIONS_PREF_KEY    "gpsagnss"
#define POWER_OPTIONS_PREF_KEY    "gpspower"
#define ADVANCED_OPTIONS_PREF_KEY "gpsadvanced"
#define URATE_OPTIONS_PREF_KEY    "gpsudate"

static const char* TAG = "gps_hw";

#define GPS_ON_OFF_PIN 8
#define GPS_UART_PORT  UART_NUM_1

#define GPS_UART_RX_PIN 4
#define GPS_UART_TX_PIN 5

static bool gps_enabled = false;
static bool gps_advanced_configured = false;

void gps_hw_init() {
#ifndef CONFIG_GPS_ENABLED
  return;
#endif
  gpio_config_t io_conf;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << GPS_ON_OFF_PIN);
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);

  gps_enabled = preferences_get_bool(GPS_ENABLED_MEM, false);

  if (gps_enabled) {
    if (!gps_hw_init_preferences(GPS_INIT_ALL)) {
      ESP_LOGE(TAG, "Failed to initialize GPS preferences");
      return;
    }
  }

  gpio_set_level(GPS_ON_OFF_PIN, gps_enabled);
}

void gps_hw_on() {
#ifndef CONFIG_GPS_ENABLED
  return;
#endif
  gpio_set_level(GPS_ON_OFF_PIN, 1);
  gps_enabled = true;

  // Configure GPS when it's enabled (if GPS is not currently active/scanning)
  // This ensures GPS is properly configured when enabled from settings
  gps_hw_init_preferences(GPS_INIT_ALL);
}

void gps_hw_off() {
#ifndef CONFIG_GPS_ENABLED
  return;
#endif
  gpio_set_level(GPS_ON_OFF_PIN, 0);
  gps_enabled = false;
}

void gsp_hw_save_state() {
#ifndef CONFIG_GPS_ENABLED
  return;
#endif
  preferences_put_bool(GPS_ENABLED_MEM, gps_enabled);
}

bool gps_hw_get_state() {
  return gps_enabled;
}

/**
 * @brief Send command to GPS module via UART and validate response
 *
 * @param command The PMTK command string to send
 * @return true if command was sent successfully, false otherwise
 */
static bool gps_hw_send_command(const char* command) {
  int len = uart_write_bytes(GPS_UART_PORT, command, strlen(command));
  if (len < 0) {
    ESP_LOGE(TAG, "Failed to send command: %s", command);
    return false;
  }

  ESP_LOGI(TAG, "Sent GPS command: %s", command);
  vTaskDelay(pdMS_TO_TICKS(100));

  return true;
}

/**
 * @brief Configure GPS for ATGM336H-6N-74 with advanced features
 *
 * Configures multi-constellation support and NMEA sentences
 */
static void gps_hw_configure_advanced(void) {
  ESP_LOGI(TAG, "Configuring GPS for ATGM336H-6N-74");

  // Wait a bit for UART to be fully ready
  vTaskDelay(pdMS_TO_TICKS(100));

  bool config_success = true;

  // Configure multi-constellation support (GPS, GLONASS, Galileo, BeiDou, QZSS)
  char constellation_cmd[] = "$PMTK353,1,1,1,1,1,0,0,0,0*2A\r\n";
  if (!gps_hw_send_command(constellation_cmd)) {
    ESP_LOGE(TAG, "Failed to configure constellations");
    config_success = false;
  }

  // Enable additional NMEA sentences for better data
  char nmea_cmd[] = "$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n";
  if (!gps_hw_send_command(nmea_cmd)) {
    ESP_LOGE(TAG, "Failed to configure NMEA sentences");
    config_success = false;
  }

  if (config_success) {
    ESP_LOGI(TAG, "GPS advanced configuration complete successfully");
  } else {
    ESP_LOGW(TAG, "GPS advanced configuration completed with errors");
  }
}

/**
 * @brief Configure the GPS update rate based on preferences
 */
static void gps_hw_configure_updaterate(void) {
  char update_rate_cmd[128];
  uint32_t update_rate = preferences_get_int(URATE_OPTIONS_PREF_KEY, 1);
  if (update_rate == GPS_UPDATE_RATE_1HZ) {
    sprintf(update_rate_cmd, "$PMTK220,1000*1F\r\n");  // 1000ms = 1Hz
    ESP_LOGI(TAG, "Change update rate to: 1 HZ");
  } else if (update_rate == GPS_UPDATE_RATE_5HZ) {
    sprintf(update_rate_cmd, "$PMTK220,200*2C\r\n");  // 200ms = 5Hz
    ESP_LOGI(TAG, "Change update rate to: 5 HZ");
  } else if (update_rate == GPS_UPDATE_RATE_10HZ) {
    sprintf(update_rate_cmd, "$PMTK220,100*2F\r\n");  // 100ms = 10Hz
    ESP_LOGI(TAG, "Change update rate to: 10 HZ");
  } else {
    ESP_LOGE(TAG, "Not supported update rate: %lu", update_rate);
    return;
  }

  if (!gps_hw_send_command(update_rate_cmd)) {
    ESP_LOGE(TAG, "Failed to set update rate");
  }
}

/**
 * @brief Enable A-GNSS (Assisted GNSS) for faster fix times
 *
 * A-GNSS uses assistance data to reduce Time To First Fix (TTFF)
 * and improve accuracy in challenging environments
 */
void gps_hw_enable_agnss(void) {
  ESP_LOGI(TAG, "Enabling A-GNSS for faster fix times");

  // Enable A-GNSS assistance data
  gps_hw_send_command("$PMTK869,1*28\r\n");

  // Enable A-GNSS data logging (optional)
  gps_hw_send_command("$PMTK869,1,1*35\r\n");

  ESP_LOGI(TAG, "A-GNSS enabled successfully");
}

/**
 * @brief Disable A-GNSS (Assisted GNSS)
 *
 * Disables assistance data, GPS will work in standalone mode
 */
void gps_hw_disable_agnss(void) {
  ESP_LOGI(TAG, "Disabling A-GNSS");

  // Disable A-GNSS assistance data
  gps_hw_send_command("$PMTK869,0*29\r\n");

  ESP_LOGI(TAG, "A-GNSS disabled successfully");
}

/**
 * @brief Set GPS power mode for power optimization
 *
 * @param mode Power mode: NORMAL, LOW_POWER, or STANDBY
 */
void gps_hw_set_power_mode(gps_power_mode_t mode) {
  const char* mode_names[] = {"NORMAL", "LOW_POWER", "STANDBY"};

  if (mode >= GPS_POWER_MODE_NORMAL && mode <= GPS_POWER_MODE_STANDBY) {
    ESP_LOGI(TAG, "Setting GPS power mode to %s", mode_names[mode]);

    switch (mode) {
      case GPS_POWER_MODE_NORMAL:
        // Normal operation mode - full performance
        gps_hw_send_command("$PMTK225,0*2B\r\n");
        break;

      case GPS_POWER_MODE_LOW_POWER:
        // Low power mode - reduced performance, lower consumption
        gps_hw_send_command("$PMTK225,1*2A\r\n");
        break;

      case GPS_POWER_MODE_STANDBY:
        // Standby mode - minimal power consumption
        gps_hw_send_command("$PMTK225,2*29\r\n");
        break;

      default:
        ESP_LOGW(TAG, "Unknown power mode: %d", mode);
        return;
    }

    ESP_LOGI(TAG, "GPS power mode set to %s successfully", mode_names[mode]);
  } else {
    ESP_LOGW(TAG, "Invalid power mode: %d (must be 0-2)", mode);
  }
}

/**
 * @brief Set the GPS update rate dynamically
 *
 * @param rate The update rate in Hz (1-10)
 */
void gps_hw_set_update_rate(uint8_t rate) {
  if (rate < 1 || rate > 10) {
    ESP_LOGW(TAG, "Invalid update rate: %d (must be 1-10 Hz)", rate);
    return;
  }

  char command[32];
  uint16_t interval_ms = 1000 / rate;

  // Calculate correct checksum for dynamic rate
  uint8_t checksum = 0;
  char temp[24];
  snprintf(temp, sizeof(temp), "PMTK220,%d", interval_ms);
  for (int i = 0; temp[i] != '\0'; i++) {
    checksum ^= temp[i];
  }

  snprintf(command, sizeof(command), "$PMTK220,%d*%02X\r\n", interval_ms,
           checksum);
  gps_hw_send_command(command);
}

/**
 * @brief Configure GPS options based on preferences and init type
 *
 * This function configures GPS settings according to the init_type parameter:
 * - GPS_INIT_ALL: Configure all options (advanced, update rate, A-GNSS, power
 * mode)
 * - GPS_INIT_ADVANCED_ONLY: Configure only advanced settings
 * - GPS_INIT_UPDATERATE_ONLY: Configure only update rate
 * - GPS_INIT_AGNSS_ONLY: Configure only A-GNSS
 * - GPS_INIT_POWER_ONLY: Configure only power mode
 * - GPS_INIT_RADIO_ONLY: Configure only radio-related settings (not used
 * currently)
 *
 * @param init_type Type of configuration to apply (see enum in gps_hw.h)
 *
 * @note Advanced configuration is only done once per boot unless reset
 */
void gps_hw_configure_options(uint8_t init_type) {
  switch (init_type) {
    case GPS_INIT_ALL:
      // Configure advanced settings (only once per boot)
      if (!gps_advanced_configured) {
        if (preferences_get_int(ADVANCED_OPTIONS_PREF_KEY, 1) == 1) {
          gps_hw_configure_advanced();
          gps_advanced_configured = true;
        }
      }
      // Configure update rate
      gps_hw_configure_updaterate();
      // Configure A-GNSS
      {
        uint16_t agnss_option = preferences_get_int(AGNSS_OPTIONS_PREF_KEY, 1);
        if (agnss_option == 0) {
          gps_hw_disable_agnss();
        } else {
          gps_hw_enable_agnss();
        }
      }
      // Configure power mode
      gps_hw_set_power_mode(preferences_get_int(POWER_OPTIONS_PREF_KEY, 0));
      break;

    case GPS_INIT_ADVANCED_ONLY:
      // Reset flag to allow reconfiguration
      gps_advanced_configured = false;
      if (preferences_get_int(ADVANCED_OPTIONS_PREF_KEY, 1) == 1) {
        gps_hw_configure_advanced();
        gps_advanced_configured = true;
      }
      break;

    case GPS_INIT_UPDATERATE_ONLY:
      gps_hw_configure_updaterate();
      break;

    case GPS_INIT_AGNSS_ONLY: {
      uint16_t agnss_option = preferences_get_int(AGNSS_OPTIONS_PREF_KEY, 1);
      if (agnss_option == 0) {
        gps_hw_disable_agnss();
      } else {
        gps_hw_enable_agnss();
      }
    } break;

    case GPS_INIT_POWER_ONLY:
      gps_hw_set_power_mode(preferences_get_int(POWER_OPTIONS_PREF_KEY, 0));
      break;

    case GPS_INIT_RADIO_ONLY:
      // Radio-only configuration (reserved for future use)
      ESP_LOGD(TAG, "GPS_INIT_RADIO_ONLY not implemented yet");
      break;

    default:
      ESP_LOGW(TAG, "Unknown GPS init type: %d, using GPS_INIT_ALL", init_type);
      // Fallback to configure all options
      if (!gps_advanced_configured) {
        if (preferences_get_int(ADVANCED_OPTIONS_PREF_KEY, 1) == 1) {
          gps_hw_configure_advanced();
          gps_advanced_configured = true;
        }
      }
      gps_hw_configure_updaterate();
      {
        uint16_t agnss_option = preferences_get_int(AGNSS_OPTIONS_PREF_KEY, 1);
        if (agnss_option == 0) {
          gps_hw_disable_agnss();
        } else {
          gps_hw_enable_agnss();
        }
      }
      gps_hw_set_power_mode(preferences_get_int(POWER_OPTIONS_PREF_KEY, 0));
      break;
  }
}

/**
 * @brief Reset the advanced configuration flag
 *
 * This allows advanced configuration to be applied again on next call
 * to gps_hw_configure_options()
 */
void gps_hw_reset_advanced_config(void) {
  gps_advanced_configured = false;
}

/**
 * @brief Initialize the UART for the GPS module
 *
 * @return true if successful, false otherwise
 */
bool gps_hw_init_preferences(uint8_t init_type) {
  uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };

  if (uart_set_pin(GPS_UART_PORT, GPS_UART_TX_PIN, GPS_UART_RX_PIN,
                   UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
    return false;
  }

  if (uart_param_config(GPS_UART_PORT, &uart_config) != ESP_OK) {
    return false;
  }

  if (uart_driver_install(GPS_UART_PORT, 2048, 2048, 0, NULL, 0) != ESP_OK) {
    return false;
  }

  gps_hw_configure_options(init_type);

  uart_driver_delete(GPS_UART_PORT);

  return true;
}
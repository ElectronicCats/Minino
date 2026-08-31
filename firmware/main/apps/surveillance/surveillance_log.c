// SPDX-License-Identifier: GPL-3.0-or-later
#include "surveillance_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sd_card.h"
#include "wardriving_common.h"

#define TAG "surv_log"

#define SURV_CSV_HEADER                                                 \
  FORMAT_VERSION                                                        \
  ",appRelease=" APP_VERSION ",model=" MODEL ",release=" RELEASE        \
  ",device=" DEVICE ",display=" DISPLAY ",board=" BOARD ",brand=" BRAND \
  ",star=" STAR ",body=" BODY ",subBody=" SUB_BODY                      \
  "\n"                                                                  \
  "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude," \
  "CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type,"   \
  "Class,Tier,Method,Score\n"

#define SURV_GPX_BUF_SZ 2048
#define MAX_RECENT_WPTS 16

static char s_csv_name[64] = SURV_DIR_NAME "/detections.csv";
static char s_gpx_name[64] = SURV_DIR_NAME "/surveillance.gpx";
static char s_buf[SURV_CSV_BUF_SZ];
static char s_gpx_buf[SURV_GPX_BUF_SZ];
static uint16_t s_lines = 0;
static bool s_log_initialized = false;
static bool s_gpx_header_written = false;
static bool s_csv_header_written = false;
static SemaphoreHandle_t s_log_mutex = NULL;

static uint8_t s_recent_wpt_macs[MAX_RECENT_WPTS][6];
static uint32_t s_recent_wpt_times[MAX_RECENT_WPTS];
static uint8_t s_recent_wpt_idx = 0;

static void log_lock(void) {
  if (s_log_mutex == NULL) {
    s_log_mutex = xSemaphoreCreateMutex();
  }
  if (s_log_mutex != NULL) {
    xSemaphoreTake(s_log_mutex, portMAX_DELAY);
  }
}

static void log_unlock(void) {
  if (s_log_mutex != NULL) {
    xSemaphoreGive(s_log_mutex);
  }
}

static const char* class_name(surv_class_t k) {
  switch (k) {
    case SURV_CLASS_FLOCK:
      return "FLOCK";
    case SURV_CLASS_FLOCK_MFR:
      return "FLOCK_MFR";
    case SURV_CLASS_ALPR:
      return "ALPR";
    case SURV_CLASS_SOUNDTHINKING:
      return "SOUNDTHINKING";
    case SURV_CLASS_AXON:
      return "AXON";
    case SURV_CLASS_GLASSES:
      return "GLASSES";
    case SURV_CLASS_CAM:
      return "CAM";
    case SURV_CLASS_AIRTAG:
      return "AIRTAG";
    case SURV_CLASS_SMARTTAG:
      return "SMARTTAG";
    case SURV_CLASS_TILE:
      return "TILE";
    case SURV_CLASS_APPLE_NEARBY:
      return "APPLE_NEARBY";
    case SURV_CLASS_IBEACON:
      return "IBEACON";
    case SURV_CLASS_ODID:
      return "ODID";
    case SURV_CLASS_SKIMMER:
      return "SKIMMER";
    case SURV_CLASS_MESHCORE:
      return "MESHCORE";
    case SURV_CLASS_RAVEN:
      return "RAVEN";
    case SURV_CLASS_PERSIST:
      return "PERSIST";
    default:
      return "UNKNOWN";
  }
}

static const char* method_name(uint8_t tier) {
  switch (tier) {
    case SURV_TIER_IE_SIG:
      return "wildcard_probe_ie_sig";
    case SURV_TIER_PROBE:
      return "wildcard_probe";
    case SURV_TIER_ADDR2:
      return "oui_addr2";
    case SURV_TIER_ADDR13:
      return "oui_addr1_addr3";
    default:
      return "ssid_kw";
  }
}

static const char* mac_str(const uint8_t mac[6]) {
  static char buf[18];
  snprintf(buf, sizeof(buf), MAC_ADDRESS_FORMAT, mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
  return buf;
}

static const char* date_str(const gps_t* gps) {
  static char buf[32];
  if (gps != NULL && (gps->valid || gps->date.year > 0)) {
    uint16_t year = gps->date.year;
    if (year < 100) {
      year += 2000;
    }
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
             year, gps->date.month, gps->date.day,
             gps->tim.hour, gps->tim.minute, gps->tim.second);
    return buf;
  }
  snprintf(buf, sizeof(buf), "uptime_%llu",
           (unsigned long long) (esp_timer_get_time() / 1000));
  return buf;
}

static uint16_t freq_of(uint8_t channel) {
  if (channel == 0) {
    return 0;  // BLE
  }
  return (uint16_t) (channel == 14 ? 2484 : 2407 + channel * 5);
}

static void append_buffered(const char* line) {
  if (line == NULL) {
    return;
  }
  log_lock();
  size_t cur_len = strlen(s_buf);
  size_t line_len = strlen(line);

  if (cur_len + line_len >= sizeof(s_buf) - 1 || s_lines >= SURV_CSV_LINES) {
    if (cur_len > 0) {
      sd_card_append_to_file(s_csv_name, s_buf);
      s_buf[0] = '\0';
      s_lines = 0;
    }
  }

  strncat(s_buf, line, sizeof(s_buf) - strlen(s_buf) - 1);
  s_lines++;
  log_unlock();
}

esp_err_t surveillance_log_begin(void) {
  log_lock();
  esp_err_t err = sd_card_mount();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "SD Card not mounted, logging to SD disabled");
    log_unlock();
    return err;
  }

  err = sd_card_create_dir(SURV_DIR_NAME);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create %s directory", SURV_DIR_NAME);
    log_unlock();
    return err;
  }

  s_buf[0] = '\0';
  s_gpx_buf[0] = '\0';
  s_lines = 0;
  memset(s_recent_wpt_times, 0, sizeof(s_recent_wpt_times));
  s_log_initialized = true;

  // Initialize CSV with Header only if file doesn't exist or is empty
  struct stat st_csv;
  if (stat(s_csv_name, &st_csv) != 0 || st_csv.st_size == 0) {
    sd_card_append_to_file(s_csv_name, (char*) SURV_CSV_HEADER);
  }
  s_csv_header_written = true;

  // Initialize GPX header only if file doesn't exist or is empty
  struct stat st_gpx;
  if (stat(s_gpx_name, &st_gpx) != 0 || st_gpx.st_size == 0) {
    const char* gpx_header =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<gpx version=\"1.1\" creator=\"Minino Surveillance\" "
        "xmlns=\"http://www.topografix.com/GPX/1/1\">\n";
    sd_card_append_to_file(s_gpx_name, (char*) gpx_header);
    s_gpx_header_written = true;
  }

  ESP_LOGI(TAG, "Surveillance log initialized at %s and %s", s_csv_name,
           s_gpx_name);
  log_unlock();
  return ESP_OK;
}

void surveillance_log_detection(const surv_event_t* ev,
                                uint8_t score,
                                const gps_t* gps) {
  if (ev == NULL || !s_log_initialized) {
    return;
  }

  // Guardar datos únicamente cuando el GPS haya triangulado una posición válida
  if (gps == NULL || !gps->valid || (gps->latitude == 0.0f && gps->longitude == 0.0f)) {
    return;
  }

  char line[SURV_CSV_LINE];
  char lat[16] = "";
  char lon[16] = "";
  double alt = gps->altitude;

  snprintf(lat, sizeof(lat), "%.6f", gps->latitude);
  snprintf(lon, sizeof(lon), "%.6f", gps->longitude);

  snprintf(line, sizeof(line),
           "%s,,,%s,%d,%u,%d,%s,%s,%.1f,%.1f,,,%s,%s,%d,%s,%d\n",
           mac_str(ev->mac), date_str(gps), ev->channel, freq_of(ev->channel),
           ev->rssi, lat, lon, alt, (double) GPS_ACCURACY,
           ev->proto == SURV_PROTO_WIFI ? "WIFI" : "BLE", class_name(ev->klass),
           ev->tier, method_name(ev->tier), score);

  append_buffered(line);
}

void surveillance_log_gpx_waypoint(const surv_event_t* ev, const gps_t* gps) {
  if (ev == NULL || gps == NULL || !gps->valid || !s_log_initialized) {
    return;
  }

  // Waypoints for confirmed high-certainty detections (Tier 3 Probe and Tier 4
  // IE Sig)
  if (ev->tier < SURV_TIER_PROBE) {
    return;
  }

  // Rate-limiting: do not duplicate waypoint for same MAC within 15 seconds
  uint32_t now_s = (uint32_t) (esp_timer_get_time() / 1000000);
  for (int i = 0; i < MAX_RECENT_WPTS; i++) {
    if (s_recent_wpt_times[i] > 0 && memcmp(s_recent_wpt_macs[i], ev->mac, 6) == 0) {
      if (now_s - s_recent_wpt_times[i] < 15) {
        return; // Skip duplicate waypoint within 15s window
      }
      break;
    }
  }

  memcpy(s_recent_wpt_macs[s_recent_wpt_idx], ev->mac, 6);
  s_recent_wpt_times[s_recent_wpt_idx] = now_s;
  s_recent_wpt_idx = (s_recent_wpt_idx + 1) % MAX_RECENT_WPTS;

  char wpt[256];
  int wpt_len = snprintf(wpt, sizeof(wpt),
           "  <wpt lat=\"%.6f\" lon=\"%.6f\">\n"
           "    <name>%s (%s)</name>\n"
           "    <desc>Tier %d %s | Ch %d | %d dBm</desc>\n"
           "    <sym>camera</sym>\n"
           "  </wpt>\n",
           gps->latitude, gps->longitude, class_name(ev->klass),
           mac_str(ev->mac), ev->tier, method_name(ev->tier), ev->channel,
           ev->rssi);

  if (wpt_len <= 0) {
    return;
  }

  log_lock();
  size_t cur_len = strlen(s_gpx_buf);
  if (cur_len + (size_t) wpt_len >= sizeof(s_gpx_buf) - 1) {
    if (cur_len > 0) {
      sd_card_append_to_file(s_gpx_name, s_gpx_buf);
      s_gpx_buf[0] = '\0';
    }
  }
  strncat(s_gpx_buf, wpt, sizeof(s_gpx_buf) - strlen(s_gpx_buf) - 1);
  log_unlock();
}

void surveillance_log_flush(void) {
  log_lock();
  if (!s_log_initialized) {
    log_unlock();
    return;
  }

  if (strlen(s_buf) > 0) {
    sd_card_append_to_file(s_csv_name, s_buf);
    s_buf[0] = '\0';
    s_lines = 0;
  }

  if (strlen(s_gpx_buf) > 0) {
    sd_card_append_to_file(s_gpx_name, s_gpx_buf);
    s_gpx_buf[0] = '\0';
  }

  if (s_gpx_header_written) {
    sd_card_append_to_file(s_gpx_name, "</gpx>\n");
    s_gpx_header_written = false;
  }
  s_csv_header_written = false;

  s_log_initialized = false;
  ESP_LOGI(TAG, "Surveillance log flushed and closed");
  log_unlock();
}

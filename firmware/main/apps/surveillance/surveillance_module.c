// SPDX-License-Identifier: GPL-3.0-or-later
#include "surveillance_module.h"
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gps_hw.h"
#include "gps_module.h"
#include "keyboard_module.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "radio_selector.h"
#include "surv_engine.h"
#include "surv_radio.h"
#include "surveillance_detect.h"
#include "surveillance_log.h"
#include "surveillance_screens.h"

#define TAG "surv_app"

// Fase persistida del mux de "Scan All". En el ESP32-C6 no caben BLE+WiFi a
// la vez y tras tocar WiFi el scan BLE ya no re-arranca (0x1) en la misma
// sesion, asi que cada ventana corre en un boot en frio: la app guarda aqui
// la fase siguiente y hace esp_restart(); al reiniciar se auto-relanza.
#define SURV_MUX_KEY        "surv_mux"
#define SURV_MUX_PHASE_BLE  1
#define SURV_MUX_PHASE_WIFI 2

static bool s_app_running = false;
static surv_event_t s_last_event;
static uint8_t s_last_score = 0;
static bool s_have_event = false;
static bool s_in_help = false;
static uint8_t s_help_page = 0;
static surv_profile_t s_profile = SURV_PROFILE_SURVEIL;
static bool s_active_scan = false;
static TaskHandle_t s_gui_task = NULL;
static gps_t s_latest_gps;
static bool s_have_gps = false;
static portMUX_TYPE s_gps_mux = portMUX_INITIALIZER_UNLOCKED;

static void surv_gps_event_cb(gps_t* gps) {
  if (gps != NULL) {
    portENTER_CRITICAL(&s_gps_mux);
    s_latest_gps = *gps;
    s_have_gps = true;
    portEXIT_CRITICAL(&s_gps_mux);
  }
}

static const char* profile_name(surv_profile_t p) {
  switch (p) {
    case SURV_PROFILE_FLOCK:
      return "FLOCK";
    case SURV_PROFILE_SURVEIL:
      return "ALL";
    case SURV_PROFILE_TRACKERS:
      return "BLE";
    default:
      return "AUTO";
  }
}

static const char* class_to_label(surv_class_t k) {
  switch (k) {
    case SURV_CLASS_FLOCK:
      return "Flock Cam";
    case SURV_CLASS_FLOCK_MFR:
      return "Flock MFR";
    case SURV_CLASS_ALPR:
      return "ALPR Cam";
    case SURV_CLASS_SOUNDTHINKING:
      return "ShotSpotter";
    case SURV_CLASS_AXON:
      return "Axon BodyCam";
    case SURV_CLASS_AIRTAG:
      return "AirTag";
    case SURV_CLASS_SMARTTAG:
      return "SmartTag";
    case SURV_CLASS_IBEACON:
      return "iBeacon";
    case SURV_CLASS_TILE:
      return "Tile";
    case SURV_CLASS_GLASSES:
      return "RayBan Meta";
    case SURV_CLASS_CAM:
      return "IP Camera";
    case SURV_CLASS_ODID:
      return "Drone ODID";
    case SURV_CLASS_SKIMMER:
      return "Skimmer";
    case SURV_CLASS_RAVEN:
      return "Raven Gunshot";
    case SURV_CLASS_PERSIST:
      return "Tracker Suspect";
    default:
      return "Surv Device";
  }
}

static void on_detection(const surv_event_t* ev, uint8_t score) {
  if (ev != NULL) {
    s_last_event = *ev;
    s_have_event = true;
    gps_t gps_snap;
    bool have_gps = false;

    portENTER_CRITICAL(&s_gps_mux);
    if (s_have_gps) {
      gps_snap = s_latest_gps;
      have_gps = true;
    }
    portEXIT_CRITICAL(&s_gps_mux);

    surveillance_log_detection(ev, score, have_gps ? &gps_snap : NULL);
    if (have_gps) {
      surveillance_log_gpx_waypoint(ev, &gps_snap);
    }
  }
  s_last_score = score;
}

static void gui_update_task(void* arg) {
  (void) arg;
  while (s_app_running) {
    if (!s_in_help) {
      uint8_t ch = surv_radio_current_channel();
      const char* label =
          s_have_event ? class_to_label(s_last_event.klass) : NULL;
      uint8_t tier = s_have_event ? s_last_event.tier : 0;
      int8_t rssi = s_have_event ? s_last_event.rssi : 0;
      uint8_t ev_ch = (s_have_event && s_last_event.proto == SURV_PROTO_WIFI)
                          ? s_last_event.channel
                          : ch;

      surveillance_screens_show_status(surv_engine_score(),
                                       profile_name(s_profile), label, tier,
                                       rssi, ev_ch, surv_queue_overflows());
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  s_gui_task = NULL;
  vTaskDelete(NULL);
}

static void start_radio_for_profile(void);

static void surveillance_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  if (s_in_help) {
    if (button_name == BUTTON_LEFT || button_name == BUTTON_BOOT ||
        button_name == BUTTON_UP) {
      s_in_help = false;
      oled_screen_clear();
    } else if (button_name == BUTTON_DOWN) {
      s_help_page = (s_help_page + 1) % 2;
      surveillance_screens_show_help(s_help_page);
    }
    return;
  }

  switch (button_name) {
    case BUTTON_LEFT:
      surveillance_module_stop();
      break;
    case BUTTON_UP:
      s_in_help = true;
      s_help_page = 0;
      surveillance_screens_show_help(s_help_page);
      break;
    case BUTTON_DOWN:
      // Rotar entre Flock (WiFi) y Trackers (BLE)
      s_profile = (s_profile == SURV_PROFILE_FLOCK) ? SURV_PROFILE_TRACKERS
                                                    : SURV_PROFILE_FLOCK;
      surv_stop();
      start_radio_for_profile();
      break;
    case BUTTON_RIGHT:
      // Toggle scan activo
      s_active_scan = !s_active_scan;
      surv_stop();
      start_radio_for_profile();
      break;
    case BUTTON_BOOT:
      s_have_event = false;
      break;
    default:
      break;
  }
}

// Arranca el stack de radio del perfil activo.
static void start_radio_for_profile(void) {
  preferences_put_uchar(SURV_MUX_KEY, 0);
  surv_begin(s_profile, s_active_scan);
}

void surveillance_module_begin_all(void) {
  s_profile = SURV_PROFILE_TRACKERS;
  surveillance_module_begin();
}

void surveillance_module_begin_flock(void) {
  s_profile = SURV_PROFILE_FLOCK;
  surveillance_module_begin();
}

void surveillance_module_begin_trackers(void) {
  s_profile = SURV_PROFILE_TRACKERS;
  surveillance_module_begin();
}

void surveillance_module_show_help(void) {
  menus_module_set_app_state(true, surveillance_input_cb);
  s_in_help = true;
  s_help_page = 0;
  surveillance_screens_show_help(0);
}

void surveillance_module_begin(void) {
  if (radio_selector_is_stack_initialized()) {
    surveillance_screens_show_radio_busy();
    menus_module_set_app_state(true, surveillance_input_cb);
    return;
  }

  s_app_running = true;
  s_in_help = false;
  s_have_event = false;
  s_last_score = 0;
  if (s_profile > SURV_PROFILE_TRACKERS || s_profile == SURV_PROFILE_SURVEIL) {
    s_profile = SURV_PROFILE_TRACKERS;
  }
  s_active_scan = preferences_get_uchar("surv_active", 0) != 0;

  // Iniciar GPS si el hardware esta habilitado
  if (gps_hw_get_state()) {
    gps_module_register_cb(surv_gps_event_cb);
    gps_module_start_scan();
  }
  surveillance_log_begin();

  menus_module_set_app_state(true, surveillance_input_cb);
  surv_register_cb(on_detection);
  start_radio_for_profile();

  if (xTaskCreate(gui_update_task, "surv_gui", 4096, NULL, 4, &s_gui_task) !=
      pdPASS) {
    ESP_LOGE(TAG, "Failed to create gui update task");
  }
}

void surveillance_module_stop(void) {
  s_app_running = false;
  ESP_LOGI(TAG, "stop: salida por back");

  // 1. Esperar a que la tarea de GUI termine para evitar colisiones I2C en la
  // pantalla OLED
  while (s_gui_task != NULL) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // 2. Detener el motor y la radio de vigilancia
  surv_stop();

  // 3. Limpiar cualquier estado persistido
  preferences_put_uchar(SURV_MUX_KEY, 0);
  surveillance_log_flush();

  if (gps_hw_get_state()) {
    gps_module_stop_read();
    gps_module_unregister_cb();
  }
  portENTER_CRITICAL(&s_gps_mux);
  s_have_gps = false;
  portEXIT_CRITICAL(&s_gps_mux);

  // 4. Restaurar el estado de menus y pantalla
  menus_module_set_default_input();
  oled_screen_clear();
  menus_module_restart();
}

void surveillance_module_boot_resume(void) {
  // Desactivado: el escaneo corre en la misma sesion sin reinicios de chip
  preferences_put_uchar(SURV_MUX_KEY, 0);
}

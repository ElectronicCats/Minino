// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_radio.h"
#include <string.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gap_dispatcher.h"
#include "radio_lease.h"
#include "surv_engine.h"
#include "surv_match.h"
#include "surv_signatures.h"

#define TAG "surv_radio"

#define SURV_DWELL_MS 250
// PHY/radio settling time between WiFi and BLE windows. On the ESP32-C6 the
// WiFi and BT drivers share the same PHY: after esp_wifi_deinit, BT controller
// re-init needs WiFi teardown to finish releasing the RF, or the BLE scan
// (SCAN_START_COMPLETE) fails with 0x1.
#define RADIO_SWITCH_DELAY_MS 300

static const uint8_t HOP_PRIMARY[] = {11, 6, 1};
static const uint8_t HOP_EXTENDED[] = {13, 8, 3};

static volatile bool s_running = false;
static surv_profile_t s_profile = SURV_PROFILE_SURVEIL;
static bool s_active_scan = false;
static volatile uint8_t s_current_channel = 1;
static TaskHandle_t s_radio_task = NULL;
// BLE scan state: desired (active window) vs controller reality
// (SCAN_START/STOP_COMPLETE). Auto-restart on INQ_CMPL_EVT must only happen if
// we actually want to scan: in SURVEIL the BLE window turns off to yield the
// air to the WiFi sniffer, and an unconditional restart kept the scan
// competing with promiscuous capture.
static volatile bool s_ble_scan_desired = false;
static volatile bool s_ble_scanning = false;
static portMUX_TYPE s_ble_mux = portMUX_INITIALIZER_UNLOCKED;
// Scan start retries within a window. Repeated 0x1 failures in a tight loop
// (set_scan_params storm) flood the log and fight the WiFi sniffer: cap and
// space out the retry chain.
#define BLE_START_MAX_RETRIES  4
#define BLE_START_RETRY_GAP_MS 300
static volatile uint8_t s_ble_start_retries = 0;

// State of the reboot-based "Scan All" mux (surv_radio_start_once): single
// BLE or WiFi window per boot; the next phase is persisted and the chip
// reboots.
static surv_radio_phase_t s_mux_phase = SURV_PHASE_BLE;
static void (*s_on_phase_done)(void) = NULL;
static volatile bool s_one_shot = false;

static esp_ble_scan_params_t s_ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x20,
    .scan_window = 0x20,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};

static bool channel_allowed(uint8_t ch) {
  wifi_country_t c;
  if (esp_wifi_get_country(&c) != ESP_OK) {
    return ch <= 11;
  }
  return ch >= c.schan && ch < (uint8_t) (c.schan + c.nchan);
}

uint8_t surv_radio_current_channel(void) {
  return s_current_channel;
}

// ble_gap_cb() runs on Bluedroid's BTC_TASK, the single task that dispatches
// every GAP/GATTS event in the system. Sleeping there stalls the whole
// Bluetooth stack: any other task calling esp_ble_gap_start_advertising(),
// stop_advertising() or a gattc operation blocks in btc_task_post() until the
// delay expires, which is what makes the UI appear to freeze when a BLE scan
// start fails while another BLE app is running. The retry therefore runs on a
// short-lived task of its own.
static void ble_scan_retry_task(void* arg) {
  (void) arg;
  vTaskDelay(pdMS_TO_TICKS(BLE_START_RETRY_GAP_MS));
  if (s_running && s_ble_scan_desired && s_profile != SURV_PROFILE_FLOCK) {
    esp_ble_gap_set_scan_params(&s_ble_scan_params);
  }
  vTaskDelete(NULL);
}

static void ble_schedule_scan_retry(void) {
  if (xTaskCreate(ble_scan_retry_task, "surv_ble_retry", 2560, NULL, 4, NULL) !=
      pdPASS) {
    ESP_LOGW(TAG, "failed to create BLE scan retry task");
  }
}

// Attempts to start scanning only if scan is desired and the controller is not
// already scanning. Avoids calling start_scanning() on an in-progress scan:
// depending on the Bluedroid version, a second start may STOP the scan instead
// of being ignored.
// NOTE: portENTER_CRITICAL/EXIT eliminates the TOCTOU race between calls from
// ble_gap_cb (Bluedroid task) and ble_window_ms (radio_task).
static void ble_try_start_scan(uint32_t seconds) {
  portENTER_CRITICAL(&s_ble_mux);
  if (!s_running || !s_ble_scan_desired || s_ble_scanning ||
      s_profile == SURV_PROFILE_FLOCK) {
    portEXIT_CRITICAL(&s_ble_mux);
    return;
  }
  s_ble_scanning = true;  // pre-set to prevent double start
  portEXIT_CRITICAL(&s_ble_mux);
  esp_err_t err = esp_ble_gap_start_scanning(seconds);
  if (err != ESP_OK) {
    portENTER_CRITICAL(&s_ble_mux);
    s_ble_scanning = false;
    portEXIT_CRITICAL(&s_ble_mux);
    ESP_LOGW(TAG, "start_scanning(%lu): %s", (unsigned long) seconds,
             esp_err_to_name(err));
    // Asynchronous retry: if start_scanning failed synchronously (e.g., scan
    // params were still pending), reconfiguring them triggers
    // SCAN_PARAM_SET_COMPLETE_EVT and retries the start. Without this, an
    // early failure leaves scanning dead for the entire session.
    if (s_running && s_ble_scan_desired && s_profile != SURV_PROFILE_FLOCK) {
      if (s_ble_start_retries < BLE_START_MAX_RETRIES) {
        s_ble_start_retries++;
        ble_schedule_scan_retry();
      }
    }
  }
}

// BLE reception totals for console diagnostics (LOG INFO, throttled to ~2 s).
// Number of advertisements seen and how many produced at least one signature
// hit. Without this, a silently dead scan (params failing to start or RF
// starved by coexistence) is indistinguishable from "no trackers nearby".
static uint32_t s_dbg_adv_seen = 0;
static uint32_t s_dbg_adv_hits = 0;
static uint32_t s_dbg_last_log_ms = 0;

static void ble_dbg_summary_locked(uint32_t now_ms) {
  const uint32_t dbg_interval = 2000;
  if ((now_ms - s_dbg_last_log_ms) >= dbg_interval || s_dbg_last_log_ms == 0) {
    ESP_LOGI(TAG, "ble rx: adv=%lu hits=%lu scanning=%d desired=%d",
             (unsigned long) s_dbg_adv_seen, (unsigned long) s_dbg_adv_hits,
             (int) s_ble_scanning, (int) s_ble_scan_desired);
    s_dbg_last_log_ms = now_ms;
  }
}

static void ble_gap_cb(esp_gap_ble_cb_event_t event,
                       esp_ble_gap_cb_param_t* param) {
  if (param == NULL) {
    return;
  }
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      if (param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "scan params OK -> start_scanning(30)");
        ble_try_start_scan(30);
      } else {
        // Without valid params, starting cannot succeed: do not call
        // start_scanning and let the ble_try_start_scan retry re-trigger
        // set_scan_params (SCAN_PARAM_SET_COMPLETE_EVT again) when due.
        ESP_LOGW(TAG, "scan params set failed: 0x%x",
                 param->scan_param_cmpl.status);
      }
      break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      s_ble_scanning = (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS);
      if (!s_ble_scanning) {
        ESP_LOGW(TAG, "BLE scan start failed: 0x%x",
                 param->scan_start_cmpl.status);
        // Spaced asynchronous retry: reconfiguring params triggers
        // SCAN_PARAM_SET_COMPLETE_EVT and retries starting, with a gap so the
        // controller can settle without flooding the log.
        if (s_running && s_ble_scan_desired &&
            s_profile != SURV_PROFILE_FLOCK) {
          if (s_ble_start_retries < BLE_START_MAX_RETRIES) {
            s_ble_start_retries++;
            ble_schedule_scan_retry();
          } else {
            ESP_LOGE(TAG, "BLE scan failed to start after %d retries (window)",
                     (int) BLE_START_MAX_RETRIES);
          }
        }
      } else {
        s_ble_start_retries = 0;
        ESP_LOGI(TAG, "BLE scan active");
      }
      break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      s_ble_scanning = false;
      ESP_LOGI(TAG, "BLE scan stopped");
      break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      esp_ble_gap_cb_param_t* scan_rst = param;
      switch (scan_rst->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT: {
          if (scan_rst->scan_rst.rssi < SURV_RSSI_MIN) {
            break;
          }
          surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
          uint8_t n = 0;
          if (scan_rst->scan_rst.adv_data_len > 0) {
            n = surv_match_ble_adv(scan_rst->scan_rst.ble_adv,
                                   scan_rst->scan_rst.adv_data_len, hits);
          }
          // Check OUI in BLE address (for Axon, etc.)
          const surv_oui_entry_t* e = surv_match_oui(scan_rst->scan_rst.bda);
          if (e != NULL && n < SURV_BLE_MAX_HITS) {
            bool already = false;
            for (uint8_t i = 0; i < n; i++) {
              if (hits[i].klass == e->klass) {
                already = true;
                break;
              }
            }
            if (!already) {
              hits[n].klass = e->klass;
              hits[n].points = e->points;
              hits[n].label = "OUI BLE";
              n++;
            }
          }

          s_dbg_adv_seen++;
          if (n > 0) {
            s_dbg_adv_hits++;
          }

          for (uint8_t i = 0; i < n; i++) {
            surv_event_t ev = {0};
            memcpy(ev.mac, scan_rst->scan_rst.bda, 6);
            ev.klass = hits[i].klass;
            // Apply tier ceiling: adv hits use ADDR2 as their base tier, but
            // if the match comes from an OUI, respect the OUI entry's
            // confidence ceiling (same as the WiFi path).
            if (e != NULL && hits[i].klass == e->klass) {
              ev.tier = surv_clamp_tier(SURV_TIER_ADDR2, e->tier);
            } else {
              ev.tier = SURV_TIER_ADDR2;
            }
            ev.rssi = scan_rst->scan_rst.rssi;
            ev.channel = 0;
            ev.proto = SURV_PROTO_BLE;
            surv_queue_push(&ev, hits[i].points);
          }
          // If neither adv nor OUI matched, record the unknown MAC for the
          // persistence tracker (detects uncatalogued trackers).
          if (n == 0) {
            uint32_t now = (uint32_t) (esp_timer_get_time() / 1000);
            surv_engine_note_unknown(scan_rst->scan_rst.bda, now);
          }

          break;
        }
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
          s_ble_scanning = false;
          ble_try_start_scan(30);
          break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
}

static void wifi_window_ms(uint32_t ms) {
  if (!s_running) {
    return;
  }
  /* Claim the shared promiscuous slot. This used to call
     esp_wifi_set_promiscuous_rx_cb()/esp_wifi_set_promiscuous() directly, which
     (a) replaced the single driver callback, so the deauth detector's
     packet_handler and the analyzer's wifi_sniffer_cb stopped seeing frames,
     and (b) turned promiscuous mode off at the end of the window, killing a
     capture somebody else was relying on. */
  if (radio_lease_prom_acquire(RADIO_PROM_SURVEILLANCE, wifi_sniffer_cb) !=
      ESP_OK) {
    ESP_LOGW(TAG, "failed to claim promiscuous mode");
    return;
  }

  uint32_t elapsed = 0;
  uint8_t hop_idx = 0;
  uint8_t round = 0;

  while (s_running && elapsed < ms) {
    uint8_t ch =
        HOP_PRIMARY[hop_idx % (sizeof(HOP_PRIMARY) / sizeof(HOP_PRIMARY[0]))];
    if (s_profile == SURV_PROFILE_SURVEIL && (round % 4 == 3)) {
      ch = HOP_EXTENDED[hop_idx %
                        (sizeof(HOP_EXTENDED) / sizeof(HOP_EXTENDED[0]))];
    }
    if (channel_allowed(ch)) {
      s_current_channel = ch;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    }
    hop_idx++;
    if (hop_idx >= 3) {
      hop_idx = 0;
      round++;
    }
    vTaskDelay(pdMS_TO_TICKS(SURV_DWELL_MS));
    elapsed += SURV_DWELL_MS;
  }

  radio_lease_prom_release(RADIO_PROM_SURVEILLANCE);
}

static void ble_window_ms(uint32_t ms) {
  if (!s_running || s_profile == SURV_PROFILE_FLOCK) {
    return;
  }
  s_current_channel = 0;
  // Mark scan desired and let GAP events synchronize the actual state.
  // In TRACKERS scanning runs continuously (auto-restart on INQ_CMPL_EVT);
  // in SURVEIL only during this window, without stealing airtime from the
  // sniffer.
  s_ble_scan_desired = true;
  ble_try_start_scan((ms + 999) / 1000);
  uint32_t elapsed = 0;
  while (s_running && elapsed < ms) {
    // Periodic diagnostics: confirms from console that scanning is running and
    // how many advertisements actually arrive (the GUI only displays hits).
    ble_dbg_summary_locked((uint32_t) (esp_timer_get_time() / 1000));
    vTaskDelay(pdMS_TO_TICKS(100));
    elapsed += 100;
  }
  if (s_profile != SURV_PROFILE_TRACKERS) {
    s_ble_scan_desired = false;
    if (s_ble_scanning) {
      esp_ble_gap_stop_scanning();
    }
  }
}

static void radio_task(void* arg);
static void scan_ble_window(uint32_t ms);

#include "wifi_controller.h"

static bool s_bt_mem_released = false;

// Releases the RAM reserved for Classic BT (BR/EDR) in the controller.
// Irreversible, one-shot; this firmware only uses BLE.
static void ble_release_classic_mem(void) {
  if (s_bt_mem_released) {
    return;
  }
  s_bt_mem_released = true;
  esp_err_t rel = esp_bt_mem_release(ESP_BT_MODE_CLASSIC_BT);
  if (rel != ESP_OK) {
    ESP_LOGW(TAG, "esp_bt_mem_release(CLASSIC_BT): %s (continuing)",
             esp_err_to_name(rel));
  }
}

// Brings up the BLE stack (controller + Bluedroid host + GAP callback) if not
// already enabled. When called after ble_controller_disable() it starts from
// scratch (controller memory returned to the heap).
static bool s_ble_claimed = false; /* we currently hold one BLE lease claim */

static void ble_claim_release(void) {
  if (s_ble_claimed) {
    s_ble_claimed = false;
    radio_lease_ble_release();
  }
}

static esp_err_t ble_controller_enable(void) {
  if (s_ble_claimed) {
    /* TRACKERS keeps its claim across windows; re-acquiring each time grew
       the refcount without bound and the stack was never freed. */
    return ESP_OK;
  }
  /* Brings up the controller and Bluedroid and takes our BLE claim. */
  esp_err_t ret = radio_lease_ble_acquire();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ble claim: %s", esp_err_to_name(ret));
    return ret;
  }
  s_ble_claimed = true;
  /* GAP registration stays here: it is ours alone, not the lease's business. */
  ret = gap_dispatcher_register(ble_gap_cb);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "gap_dispatcher_register: %s", esp_err_to_name(ret));
    ble_claim_release();
    return ret;
  }
  return ESP_OK;
}

// Shuts down the entire BLE stack and returns its memory to the heap. Used by
// the SURVEIL profile between windows: on the ESP32-C6 BLE + Wi-Fi do not fit
// enabled at the same time, and previously this init failed with ESP_ERR_NO_MEM
// depending on order (BLE first made Wi-Fi fail; Wi-Fi first made BLE fail).
static void ble_controller_disable(void) {
  s_ble_scan_desired = false;
  if (s_ble_scanning) {
    esp_ble_gap_stop_scanning();
    s_ble_scanning = false;
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  /* Full Bluedroid + controller teardown is global. Every other BLE app holds a
     radio_lease BLE claim for as long as it is running, and this used to pull
     the stack out from under BLE spam, BLE HID, the trackers scanner and the
     gatt console commands every time a window ended. Only the last claim may
     take it down. */
  if (radio_lease_ble_shared()) {
    ESP_LOGD(TAG, "another module is still using BLE (%u); keeping stack up",
             (unsigned) radio_lease_ble_count());
    return;
  }
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
    esp_bluedroid_disable();
  }
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
    esp_bluedroid_deinit();
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    esp_bt_controller_disable();
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    esp_bt_controller_deinit();
  }
}

// BLE window: ensures the stack is up, starts scanning, and holds it for `ms`.
// In SURVEIL, when finished, shuts down the full stack to hand RAM back to the
// 802.11 sniffer for the next window.
static void scan_ble_window(uint32_t ms) {
  if (!s_running || s_profile == SURV_PROFILE_FLOCK) {
    return;
  }
  if (ble_controller_enable() != ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(500));
    return;
  }
  s_ble_start_retries = 0;
  s_ble_scan_params.scan_type =
      s_active_scan ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE;
  s_ble_scan_desired = true;
  // Give the re-initialized controller time to settle the PHY before
  // requesting the scan.
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_ble_gap_set_scan_params(&s_ble_scan_params);
  ble_window_ms(ms);
  if (s_profile == SURV_PROFILE_SURVEIL) {
    /* Release our own claim first, so ble_controller_disable() sees the real
       number of other users and only tears the stack down if we were the last
       one (which is the case that actually needs the RAM back). */
    ble_claim_release();
    ble_controller_disable();
  }
}

static void radio_task(void* arg) {
  (void) arg;
  while (s_running) {
    switch (s_profile) {
      case SURV_PROFILE_FLOCK:
        // Wi-Fi was started in surv_radio_start and stays on: the loop does not
        // shut it down.
        wifi_window_ms(20000);
        break;
      case SURV_PROFILE_SURVEIL:
        scan_ble_window(6000);
        if (!s_running) {
          break;
        }
        /* Wi-Fi driver hand-over, symmetric acquire/release.
           The claim only actually deinits the driver when nobody else holds
           one: the analyzer, the deauth detector, the DroneID scanner and the
           spectrum app all run on the same driver, and taking it down
           mid-session was killing their captures. */
        vTaskDelay(pdMS_TO_TICKS(RADIO_SWITCH_DELAY_MS));
        if (!s_running) {
          break;
        }
        if (radio_lease_wifi_acquire(RADIO_WIFI_MODE_STA) != ESP_OK) {
          ESP_LOGW(TAG, "wifi window skipped: the radio is held by BLE");
          vTaskDelay(pdMS_TO_TICKS(500));
          break;
        }
        wifi_window_ms(14000);
        radio_lease_wifi_release();
        vTaskDelay(pdMS_TO_TICKS(RADIO_SWITCH_DELAY_MS));
        break;
      case SURV_PROFILE_TRACKERS:
        scan_ble_window(20000);
        break;
    }
    if (s_active_scan && s_profile != SURV_PROFILE_TRACKERS && s_running) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
  s_radio_task = NULL;
  vTaskDelete(NULL);
}

esp_err_t surv_radio_start(surv_profile_t p, bool active_scan) {
  s_profile = p;
  s_active_scan = active_scan;
  s_ble_scan_desired = false;
  s_ble_scanning = false;

  // Stacks are managed window by window in radio_task (in SURVEIL, BLE + Wi-Fi
  // do not fit in RAM at once; in TRACKERS, Wi-Fi left over from a previous
  // profile would tie up the shared radio). Here we only prepare the ground:
  ble_release_classic_mem();
  //  - TRACKERS: ensure Wi-Fi is off if coming from another profile.
  if (p == SURV_PROFILE_TRACKERS) {
    /* Our own claim first; wifi_driver_deinit_if_started() only runs if the
       release dropped the count to zero. */
    radio_lease_wifi_release();
    wifi_driver_deinit_if_started();
    // On ESP32-C6 WiFi and BT share the PHY: after esp_wifi_deinit the
    // teardown must finish releasing RF before radio_task powers on the BT
    // controller, or the BLE scan (SCAN_START_COMPLETE) fails with 0x1.
    // Same rule as the SURVEIL inter-window delay (RADIO_SWITCH_DELAY_MS).
    vTaskDelay(pdMS_TO_TICKS(RADIO_SWITCH_DELAY_MS));
  }
  //  - FLOCK: bring up Wi-Fi once; the loop keeps it on.
  if (p == SURV_PROFILE_FLOCK) {
    /* Held for the whole profile; released by surv_radio_stop(). */
    radio_lease_wifi_acquire(RADIO_WIFI_MODE_STA);
  }

  s_running = true;
  if (xTaskCreate(radio_task, "surv_radio", 4096, NULL, 5, &s_radio_task) !=
      pdPASS) {
    s_running = false;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

void surv_radio_stop(void) {
  s_ble_scan_desired = false;
  s_running = false;
  // Cancel any in-flight mux: without a callback, no phase flip or spurious
  // esp_restart() occurs.
  s_one_shot = false;
  s_on_phase_done = NULL;
  radio_lease_prom_release(RADIO_PROM_SURVEILLANCE);
  ble_claim_release();
  radio_lease_wifi_release();
  esp_ble_gap_stop_scanning();
  // Wait for radio_task to finish and self-delete. Without this, returning
  // early causes race conditions on radio resources (WiFi promiscuous, BLE
  // GAP).
  gap_dispatcher_unregister(ble_gap_cb);
  while (s_radio_task != NULL) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------------------------------------------------------------------
// Reboot-based "Scan All" mux.
//
// On the ESP32-C6 BLE + WiFi cannot both stay initialized at once (RAM), and
// after touching WiFi, re-initializing the BLE controller can no longer start
// a scan (SCAN_START_COMPLETE 0x1), even with SW coex disabled: it is
// irreversible within the session. The only known-good state for each radio is
// being the first user on a fresh boot. Therefore SURVEIL multiplexes phases
// across chip reboots: each phase cold-starts with a SINGLE stack (BLE or
// WiFi), runs its window, and hands control to the next boot.
// ---------------------------------------------------------------------------

#define MUX_BLE_WINDOW_MS  6000
#define MUX_WIFI_WINDOW_MS 14000

// Single-iteration task: runs the phase window and, if not stopped, notifies
// surveillance_module to flip the phase and reboot.
static void mux_radio_task(void* arg) {
  (void) arg;
  while (s_running) {
    if (s_mux_phase == SURV_PHASE_BLE) {
      scan_ble_window(MUX_BLE_WINDOW_MS);
    } else {
      radio_lease_wifi_acquire(RADIO_WIFI_MODE_STA);
      wifi_window_ms(MUX_WIFI_WINDOW_MS);
      radio_lease_wifi_release();
    }
    break;
  }
  const bool fire =
      s_running;  // false if surv_radio_stop() cancelled the phase
  s_running = false;
  s_one_shot = false;
  s_radio_task = NULL;
  void (*cb)(void) = s_on_phase_done;
  s_on_phase_done = NULL;
  if (fire && cb != NULL) {
    cb();  // flips the persisted phase and calls esp_restart() from the app
           // layer
  }
  vTaskDelete(NULL);
}

esp_err_t surv_radio_start_once(surv_profile_t p,
                                bool active_scan,
                                surv_radio_phase_t phase,
                                void (*on_phase_done)(void)) {
  if (p != SURV_PROFILE_SURVEIL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_radio_task != NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  s_profile = p;
  s_active_scan = active_scan;
  s_ble_scan_desired = false;
  s_ble_scanning = false;
  s_ble_start_retries = 0;
  s_mux_phase = phase;
  s_on_phase_done = on_phase_done;
  ble_release_classic_mem();
  // In the BLE phase a cold boot is the working state; in the WiFi phase BLE
  // is never touched. No pre-deinit here.
  s_running = true;
  s_one_shot = true;
  if (xTaskCreate(mux_radio_task, "surv_mux", 4096, NULL, 5, &s_radio_task) !=
      pdPASS) {
    s_running = false;
    s_radio_task = NULL;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

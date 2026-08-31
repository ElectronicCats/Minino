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
#include "surv_engine.h"
#include "surv_match.h"
#include "surv_signatures.h"

#define TAG "surv_radio"

#define SURV_DWELL_MS 250
// Tiempo de asentamiento del PHY/radio entre ventanas WiFi y BLE. En el
// ESP32-C6 los drivers WiFi y BT comparten el mismo PHY: tras esp_wifi_deinit
// la re-init del controlador BT necesita que la teardown WiFi termine de
// soltar el RF, o el scan BLE (SCAN_START_COMPLETE) falla con 0x1.
#define RADIO_SWITCH_DELAY_MS 300

static const uint8_t HOP_PRIMARY[] = {11, 6, 1};
static const uint8_t HOP_EXTENDED[] = {13, 8, 3};

static volatile bool s_running = false;
static surv_profile_t s_profile = SURV_PROFILE_SURVEIL;
static bool s_active_scan = false;
static volatile uint8_t s_current_channel = 1;
static TaskHandle_t s_radio_task = NULL;
// Estado del escaneo BLE: deseo (ventana activa) vs realidad del controlador
// (SCAN_START/STOP_COMPLETE). El auto-restart en INQ_CMPL_EVT solo debe
// ocurrir si de verdad queremos escanear: en SURVEIL la ventana BLE se apaga
// para cederle el aire al sniffer WiFi, y un restart incondicional mantenia
// el escaneo compitiendo con la captura promiscua.
static volatile bool s_ble_scan_desired = false;
static volatile bool s_ble_scanning = false;
static portMUX_TYPE s_ble_mux = portMUX_INITIALIZER_UNLOCKED;
// Reintentos de arranque del scan dentro de una ventana. Un fallo 0x1
// repetido a lo loco (storm de set_scan_params) inunda el log y pelea con el
// sniffer WiFi: limite y cadena de reintentos espaciados en el tiempo.
#define BLE_START_MAX_RETRIES  4
#define BLE_START_RETRY_GAP_MS 300
static volatile uint8_t s_ble_start_retries = 0;

// Estado del mux de "Scan All" por reinicio (surv_radio_start_once): ventana
// unica BLE o WiFi por boot; la fase siguiente se persiste y el chip reinicia.
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

// Intenta arrancar el escaneo solo si hay deseo activo y el controlador aun
// no esta escaneando. Evita llamar start_scanning() sobre un escaneo ya en
// curso: segun la version de Bluedroid, el segundo start puede DETENER el
// escaneo en vez de ignorarse.
// NOTA: portENTER_CRITICAL/EXIT para eliminar la carrera TOCTOU entre la
// llamada desde ble_gap_cb (tarea Bluedroid) y ble_window_ms (radio_task).
static void ble_try_start_scan(uint32_t seconds) {
  portENTER_CRITICAL(&s_ble_mux);
  if (!s_running || !s_ble_scan_desired || s_ble_scanning ||
      s_profile == SURV_PROFILE_FLOCK) {
    portEXIT_CRITICAL(&s_ble_mux);
    return;
  }
  s_ble_scanning = true;  // pre-set para prevenir doble start
  portEXIT_CRITICAL(&s_ble_mux);
  esp_err_t err = esp_ble_gap_start_scanning(seconds);
  if (err != ESP_OK) {
    portENTER_CRITICAL(&s_ble_mux);
    s_ble_scanning = false;
    portEXIT_CRITICAL(&s_ble_mux);
    ESP_LOGW(TAG, "start_scanning(%lu): %s", (unsigned long) seconds,
             esp_err_to_name(err));
    // Reintento asincrono: si start_scanning fallo de forma sincronica (p.ej.
    // los scan params aun estaban pendientes), reconfigurarlos dispara
    // SCAN_PARAM_SET_COMPLETE_EVT y vuelve a intentar el arranque. Sin esto,
    // un fallo temprano deja el escaneo muerto para toda la sesion.
    if (s_running && s_ble_scan_desired && s_profile != SURV_PROFILE_FLOCK) {
      if (s_ble_start_retries < BLE_START_MAX_RETRIES) {
        s_ble_start_retries++;
        vTaskDelay(pdMS_TO_TICKS(BLE_START_RETRY_GAP_MS));
        esp_ble_gap_set_scan_params(&s_ble_scan_params);
      }
    }
  }
}

// Totales de recepcion BLE para diagnostico desde consola (LOG INFO,
// throttled a ~2 s). Numero de advertisements vistos y cuantos produjeron al
// menos un hit de firma. Sin esto un escaneo muerto en silencio (parametros
// que no arrancan o RF asfixiado por coexistencia) es indistinguible de "no
// hay trackers cerca".
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
        // Sin params validos el arranque no puede triunfar: no llamar a
        // start_scanning y dejar que el reintento de ble_try_start_scan
        // re-dispare set_scan_params (SCAN_PARAM_SET_COMPLETE_EVT de nuevo)
        // cuando toque.
        ESP_LOGW(TAG, "scan params set fallo: 0x%x",
                 param->scan_param_cmpl.status);
      }
      break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      s_ble_scanning = (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS);
      if (!s_ble_scanning) {
        ESP_LOGW(TAG, "BLE scan start fallo: 0x%x",
                 param->scan_start_cmpl.status);
        // Reintento asincrono espaciado: reconfigurar params dispara
        // SCAN_PARAM_SET_COMPLETE_EVT y vuelve a intentar arrancar, pero con
        // un gap para que el controlador asiente y sin inundar el log.
        if (s_running && s_ble_scan_desired &&
            s_profile != SURV_PROFILE_FLOCK) {
          if (s_ble_start_retries < BLE_START_MAX_RETRIES) {
            s_ble_start_retries++;
            vTaskDelay(pdMS_TO_TICKS(BLE_START_RETRY_GAP_MS));
            esp_ble_gap_set_scan_params(&s_ble_scan_params);
          } else {
            ESP_LOGE(TAG, "BLE scan no arranca tras %d reintentos (ventana)",
                     (int) BLE_START_MAX_RETRIES);
          }
        }
      } else {
        s_ble_start_retries = 0;
        ESP_LOGI(TAG, "BLE scan activo");
      }
      break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      s_ble_scanning = false;
      ESP_LOGI(TAG, "BLE scan detenido");
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
          // Checar OUI en direccion BLE (para Axon, etc.)
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
            // Aplicar techo de tier: los hits de adv usan ADDR2 como tier
            // base, pero si el match viene de OUI, respetar el techo de
            // confianza de la entrada OUI (igual que la ruta WiFi).
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
          // Si no hubo hits de adv ni de OUI, registrar MAC desconocida para
          // el rastreador de persistencia (detecta trackers no catalogados).
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
  esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb);
  esp_wifi_set_promiscuous(true);

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

  esp_wifi_set_promiscuous(false);
}

static void ble_window_ms(uint32_t ms) {
  if (!s_running || s_profile == SURV_PROFILE_FLOCK) {
    return;
  }
  s_current_channel = 0;
  // Marcar deseo y dejar que los eventos GAP sincronicen el estado real.
  // En TRACKERS el escaneo vive continuo (auto-restart en INQ_CMPL_EVT);
  // en SURVEIL solo durante esta ventana, sin robarle aire al sniffer.
  s_ble_scan_desired = true;
  ble_try_start_scan((ms + 999) / 1000);
  uint32_t elapsed = 0;
  while (s_running && elapsed < ms) {
    // Diagnostico periodico: confirma desde consola que el escaneo corre y
    // cuantos advertisements de verdad llegan (el GUI solo muestra hits).
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

// Libera la RAM que el stack clasico (BR/EDR) tiene reservada en el
// controlador. Irreversible, una sola vez; este firmware solo usa BLE.
static void ble_release_classic_mem(void) {
  if (s_bt_mem_released) {
    return;
  }
  s_bt_mem_released = true;
  esp_err_t rel = esp_bt_mem_release(ESP_BT_MODE_CLASSIC_BT);
  if (rel != ESP_OK) {
    ESP_LOGW(TAG, "esp_bt_mem_release(CLASSIC_BT): %s (continuando)",
             esp_err_to_name(rel));
  }
}

// Enciende el stack BLE (controlador + host Bluedroid + callback GAP) si no
// estaba ya encendido. Si se lllama tras ble_controller_disable() vuelve a
// partir de cero (memoria del controlador devuelta al heap).
static esp_err_t ble_controller_enable(void) {
  esp_err_t ret;
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_init: %s", esp_err_to_name(ret));
      return ret;
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_enable: %s", esp_err_to_name(ret));
      return ret;
    }
  }
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bd_cfg);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_init_with_cfg: %s", esp_err_to_name(ret));
      return ret;
    }
  }
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_enable: %s", esp_err_to_name(ret));
      return ret;
    }
  }
  ret = gap_dispatcher_register(ble_gap_cb);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "gap_dispatcher_register: %s", esp_err_to_name(ret));
    return ret;
  }
  return ESP_OK;
}

// Apaga el stack BLE entero y devuelve su memoria al heap. La usa el perfil
// SURVEIL entre ventanas: en el ESP32-C6 no caben BLE + Wi-Fi encendidos a la
// vez, y antes este init fallaba con ESP_ERR_NO_MEM segun el orden (con BLE
// primero fallaba Wi-Fi; con Wi-Fi primero fallaba BLE).
static void ble_controller_disable(void) {
  s_ble_scan_desired = false;
  if (s_ble_scanning) {
    esp_ble_gap_stop_scanning();
    s_ble_scanning = false;
    vTaskDelay(pdMS_TO_TICKS(50));
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

// Ventana BLE: asegura el stack, arranca el escaneo y lo mantiene `ms`.
// En SURVEIL, al terminar, apaga el stack completo para darle la RAM al
// sniffer 802.11 de la ventana siguiente.
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
  // Dar al controlador re-inicializado tiempo de asentar el PHY antes de
  // pedirle el scan.
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_ble_gap_set_scan_params(&s_ble_scan_params);
  ble_window_ms(ms);
  if (s_profile == SURV_PROFILE_SURVEIL) {
    ble_controller_disable();
  }
}

static void radio_task(void* arg) {
  (void) arg;
  while (s_running) {
    switch (s_profile) {
      case SURV_PROFILE_FLOCK:
        // Wi-Fi se inicio en surv_radio_start y se queda: el loop no lo apaga.
        wifi_window_ms(20000);
        break;
      case SURV_PROFILE_SURVEIL:
        scan_ble_window(6000);
        if (!s_running) {
          break;
        }
        if (wifi_driver_get_initialized()) {
          wifi_driver_deinit();
        }
        vTaskDelay(pdMS_TO_TICKS(RADIO_SWITCH_DELAY_MS));
        if (!s_running) {
          break;
        }
        if (!wifi_driver_get_initialized()) {
          wifi_driver_init_sta();
        }
        wifi_window_ms(14000);
        if (wifi_driver_get_initialized()) {
          wifi_driver_deinit();
        }
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

  // Los stacks se gestionan ventana a ventana en radio_task (en SURVEIL no
  // caben BLE + Wi-Fi encendidos a la vez; en TRACKERS el Wi-Fi de un perfil
  // anterior enredaria el radio compartido). Aqui solo se prepara el terreno:
  ble_release_classic_mem();
  //  - TRACKERS: asegurar que Wi-Fi queda fuera si venimos de otro perfil.
  if (p == SURV_PROFILE_TRACKERS) {
    wifi_driver_deinit_if_started();
    // En el ESP32-C6 WiFi y BT comparten PHY: tras esp_wifi_deinit hace falta
    // que la teardown termine de soltar el RF antes de que radio_task encienda
    // el controlador BT, o el scan BLE (SCAN_START_COMPLETE) falla con 0x1.
    // Misma regla que el inter-window de SURVEIL (RADIO_SWITCH_DELAY_MS).
    vTaskDelay(pdMS_TO_TICKS(RADIO_SWITCH_DELAY_MS));
  }
  //  - FLOCK: prender Wi-Fi una vez; el loop lo mantiene encendido.
  if (p == SURV_PROFILE_FLOCK && !wifi_driver_get_initialized()) {
    wifi_driver_init_sta();
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
  // Cancelar un eventual mux en curso: sin callback no se flipea fase ni hay
  // esp_restart() espurio.
  s_one_shot = false;
  s_on_phase_done = NULL;
  esp_wifi_set_promiscuous(false);
  esp_ble_gap_stop_scanning();
  gap_dispatcher_unregister(ble_gap_cb);
  // Esperar a que la tarea radio_task termine y se auto-elimine. Sin esto,
  // surv_radio_start() podria ejecutarse antes de que la tarea anterior haya
  // liberado el CPU, provocando condiciones de carrera en los recursos de
  // radio (WiFi promiscuo, BLE GAP).
  while (s_radio_task != NULL) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------------------------------------------------------------------
// Mux de "Scan All" por reinicio.
//
// En el ESP32-C6 no caben BLE + WiFi inicializados a la vez (RAM), y despues
// de tocar WiFi la re-init del controlador BLE ya no puede arrancar el scan
// (SCAN_START_COMPLETE 0x1), ni siquiera con SW coex apagado: es irreversible
// dentro de la sesion. El unico estado conocido-bueno para cada radio es el
// primer usuario de cada boot. Por eso SURVEIL multiplexa por fases con
// reinicio del chip: cada fase arranca en frio con UN solo stack (BLE o WiFi),
// corre su ventana y entrega el control al siguiente boot.
// ---------------------------------------------------------------------------

#define MUX_BLE_WINDOW_MS  6000
#define MUX_WIFI_WINDOW_MS 14000

// Tarea de una sola iteracion: ejecuta la ventana de la fase y, si nadie la
// detuvo, le avisa a surveillance_module para flipar fase y reiniciar.
static void mux_radio_task(void* arg) {
  (void) arg;
  while (s_running) {
    if (s_mux_phase == SURV_PHASE_BLE) {
      scan_ble_window(MUX_BLE_WINDOW_MS);
    } else {
      if (!wifi_driver_get_initialized()) {
        wifi_driver_init_sta();
      }
      wifi_window_ms(MUX_WIFI_WINDOW_MS);
      if (wifi_driver_get_initialized()) {
        wifi_driver_deinit();
      }
    }
    break;
  }
  const bool fire = s_running;  // false si surv_radio_stop() cancelo la fase
  s_running = false;
  s_one_shot = false;
  s_radio_task = NULL;
  void (*cb)(void) = s_on_phase_done;
  s_on_phase_done = NULL;
  if (fire && cb != NULL) {
    cb();  // flipa la fase persistida y hace esp_restart() desde la capa app
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
  // En la fase BLE el arranque en frio del boot es el estado que funciona;
  // en la fase WiFi BLE nunca se toca. Ningun pref-deinit aqui.
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

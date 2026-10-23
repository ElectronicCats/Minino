/* WiFi Service singleton: encapsula esp_wifi_init() y provee un API común.
 *
 * Antes: 13 módulos llamaban a esp_wifi_init() directamente, cada uno con
 * su propio sdkconfig (#ifdef CONFIG_WIFI_*_ENABLE). Resultado: recompilación
 * condicional, duplicación de lógica y inicializaciones redundantes.
 *
 * Después: un único punto de entrada que inicializa el stack y expone un
 * manejador estable para el resto del firmware. Los cambios de estado se
 * notifican con callbacks directos en cada app; no hay bus de eventos.
 *
 * Thread-safety: la inicialización y el estado se protegen con un spinlock
 * propio. Los getters de modo consultan al driver, así que no quedan
 * obsoletos cuando una app llama a esp_wifi_set_mode().
 */

#pragma once

#include "esp_wifi.h"

/* Estado interno, protegido por s_wifi_mux. */
typedef struct {
  wifi_mode_t mode;
  wifi_interface_t iface;
  bool is_up;
  bool is_sta;
  bool is_ap;
  wifi_second_chan_t primary_chan;
  wifi_second_chan_t secondary_chan;
} wifi_service_state_t;

/* Optional gate consulted before esp_wifi_init(). It lets the radio policy
 * (radio_lease) veto a bring-up while BLE owns the PHY, without making this
 * component depend on radio_lease. */
typedef bool (*wifi_service_init_guard_t)(void);
void wifi_service_set_init_guard(wifi_service_init_guard_t guard);

/* API pública. */
bool wifi_service_init(wifi_mode_t mode);
bool wifi_service_deinit(void);

/* Cambia el modo y actualiza el estado del servicio.
 * No inicializa el driver: wifi_service_init() debe haber corrido antes. */
esp_err_t wifi_service_set_mode(wifi_mode_t mode);

/* Estado consultable. El modo y los flags STA/AP se leen del driver. */
wifi_mode_t wifi_service_mode_get(void);
wifi_interface_t wifi_service_iface_get(void);
bool wifi_service_is_up_get(void);
bool wifi_service_is_sta_get(void);
bool wifi_service_is_ap_get(void);
wifi_second_chan_t wifi_service_primary_chan_get(void);
wifi_second_chan_t wifi_service_secondary_chan_get(void);

/* --- Helpers de conveniencia (opcionales) --- */

#define wifi_service_is_any_active() \
  (wifi_service_is_sta_get() || wifi_service_is_ap_get())

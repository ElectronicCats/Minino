/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Shared ownership of the single 2.4 GHz radio.
 *
 * Two independent locks, both reference counted:
 *
 *  - **Promiscuous mode.** ESP-IDF keeps one promiscuous receive callback and
 *    one on/off switch. Six subsystems need it. They used to call
 *    esp_wifi_set_promiscuous_rx_cb()/esp_wifi_set_promiscuous() directly, so
 *    the last one to start stole the callback from the others and a window
 *    ending with esp_wifi_set_promiscuous(false) killed a capture that somebody
 *    else was relying on. Now each consumer claims a slot and the driver is only
 *    touched on the 0->1 and 1->0 transitions; frames are fanned out to every
 *    holder.
 *
 *  - **The WiFi driver and the BLE stack.** The surveillance detector rotates
 *    between a WiFi window and a BLE window and used to esp_wifi_deinit() /
 *    esp_bluedroid_deinit() in between. Those are global: any app that merely
 *    happened to be running - the analyzer's sniffer, the deauth detector, BLE
 *    spam, BLE HID, the trackers scanner - lost the radio mid-operation. They
 *    now hold a lease, and the detector only takes the driver down when it is
 *    the last holder.
 *
 *  WiFi and BLE are mutually exclusive on the ESP32-C6: their stacks do not
 *  fit in internal SRAM at the same time. A request for one while the other has
 *  holders returns ESP_ERR_NOT_SUPPORTED instead of tearing the active stack
 *  down under its owner. Apps that need both serialize them in windows.
 */
#pragma once

#include "sdkconfig.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Must stay in sync with radio_prom_user_t below. */
typedef enum {
  RADIO_PROM_NONE = 0,
  RADIO_PROM_SURVEILLANCE,
  RADIO_PROM_SNIFFER,
  RADIO_PROM_DEAUTH_DETECTOR,
  RADIO_PROM_DRONEID_SCANNER,
  RADIO_PROM_SPECTRUM,
  RADIO_PROM_SDR_RX,
  RADIO_PROM_USER_COUNT
} radio_prom_user_t;

/** Name of a consumer, for logs. */
const char* radio_lease_user_name(radio_prom_user_t user);

/**
 * @brief Claim promiscuous mode and register `cb` as this consumer's receiver
 *
 * The driver is switched on only when the first consumer arrives.
 *
 * @return ESP_ERR_INVALID_ARG for an out-of-range id or NULL callback,
 *         ESP_ERR_INVALID_STATE when the table is full
 */
esp_err_t radio_lease_prom_acquire(radio_prom_user_t user,
                                   wifi_promiscuous_cb_t cb);

/**
 * @brief Hold promiscuous mode without registering a receiver
 *
 * For consumers that only need the driver's PHY clocks and calibration to stay
 * alive (the SDR RX engine) and do not care about the frames.
 */
esp_err_t radio_lease_prom_hold(radio_prom_user_t user);

/**
 * @brief Replace the receiver of a consumer that already holds the radio
 */
esp_err_t radio_lease_prom_set_cb(radio_prom_user_t user,
                                  wifi_promiscuous_cb_t cb);

/**
 * @brief Give up promiscuous mode
 *
 * The driver is switched off only when the last consumer leaves.
 *
 * @return ESP_ERR_NOT_FOUND when `user` did not hold it
 */
esp_err_t radio_lease_prom_release(radio_prom_user_t user);

/** How many consumers currently hold promiscuous mode. */
uint8_t radio_lease_prom_count(void);

/** True when `user` currently holds promiscuous mode. */
bool radio_lease_prom_held(radio_prom_user_t user);

/**
 * @brief Apply a promiscuous filter
 *
 * The filter is global to the driver, so the most recent caller wins - exactly
 * as before this layer existed. What changed is that installing a filter no
 * longer forces promiscuous mode on.
 */
esp_err_t radio_lease_prom_set_filter(const wifi_promiscuous_filter_t* filter);

/**
 * @brief Claim the WiFi driver for `mode`
 *
 * The driver is only initialised on the 0->1 transition. A second consumer
 * asking for a different mode is honoured only in the sense that it does not
 * tear anything down; the mode of the first holder stands and the mismatch is
 * logged.
 *
 * @return ESP_ERR_NOT_SUPPORTED when BLE has holders, or the bring-up error.
 */
typedef enum {
  RADIO_WIFI_MODE_STA = 0,
  RADIO_WIFI_MODE_NULL,
  RADIO_WIFI_MODE_APSTA,
} radio_wifi_init_mode_t;

esp_err_t radio_lease_wifi_acquire(radio_wifi_init_mode_t mode);

/** Drop a WiFi driver claim; deinitialises only when the last one goes. */
void radio_lease_wifi_release(void);

/** True when somebody other than the caller holds a WiFi driver claim. */
bool radio_lease_wifi_shared(void);

/** Current number of WiFi driver claims. */
uint8_t radio_lease_wifi_count(void);

#if CONFIG_BT_ENABLED
/**
 * @brief Claim the BLE stack
 *
 * Brings up the BT controller and Bluedroid on the 0->1 transition. Does *not*
 * register a GAP callback: that stays with gap_dispatcher, per subscriber.
 *
 * @return ESP_ERR_NOT_SUPPORTED when WiFi has holders. The WiFi driver is only
 *         deinitialised when nobody holds it.
 */
bool radio_lease_wifi_allowed(void);

esp_err_t radio_lease_ble_acquire(void);
esp_err_t radio_lease_ble_acquire_timeout(TickType_t timeout_ticks);

esp_err_t radio_lease_wifi_acquire_timeout(radio_wifi_init_mode_t mode, TickType_t timeout_ticks);

/**
 * @brief Drop a BLE stack claim
 *
 * Tears the stack down only when the last claim goes. Every BLE app has to hold
 * a claim for the duration of its session, otherwise the surveillance detector's
 * inter-window teardown would pull the stack out from under it.
 */
void radio_lease_ble_release(void);

/** True when somebody else holds a BLE stack claim. */
bool radio_lease_ble_shared(void);

/** Current number of BLE stack claims. */
uint8_t radio_lease_ble_count(void);

/**
 * @brief Claim exclusive use of the BLE radio for advertising
 *
 * The controller cannot scan and advertise/set a random address at the same
 * time: "Advertising or scanning now, can't set randaddress". An app that
 * transmits (BLE spam, BLE HID) must first take this gate, stop whatever scan
 * is in flight and then do its own GAP work.
 *
 * While the gate is held, every scanner in the tree (bt_gattc consumers,
 * gattcmd, the ADV scanner) consults radio_lease_ble_scan_allowed() before
 * (re)starting a scan, so a scan stopped by the exclusive owner is never
 * re-armed behind its back.
 *
 * Refcounted: take it on entry, give it back on exit. It is purely advisory
 * about scanning; the BLE stack lease is independent.
 */
void radio_lease_ble_block_scan(void);

/**
 * @brief Give up the exclusive-scanning gate taken by the caller
 */
void radio_lease_ble_unblock_scan(void);

/** True while BLE apps may scan (i.e. no advertising-exclusive app is up). */
bool radio_lease_ble_scan_allowed(void);
#endif

#ifdef __cplusplus
}
#endif
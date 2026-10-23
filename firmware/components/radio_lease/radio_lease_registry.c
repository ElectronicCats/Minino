/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "radio_lease_registry.h"

#include <string.h>

#include "freertos/FreeRTOS.h"

/* Guards the consumer table. The registry itself is per-instance and small;
   the lock only has to stop a frame being dispatched to a half-updated entry.
 */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void radio_lease_registry_reset(radio_lease_registry_t* reg) {
  if (reg == NULL) {
    return;
  }
  memset(reg, 0, sizeof(*reg));
}

esp_err_t radio_lease_registry_acquire(radio_lease_registry_t* reg,
                                       uint8_t user,
                                       radio_lease_rx_fn fn,
                                       void* ctx) {
  if (reg == NULL || fn == NULL || user >= RADIO_LEASE_MAX_USERS) {
    return ESP_ERR_INVALID_ARG;
  }

  /* The slot IS the consumer id: a re-acquire with a different receiver is a
     legitimate update (the WiFi sniffer registers its callback on every start),
     not a second holder. Storing it in "the first free slot" instead would make
     holds()/release() look for an entry that was never written. */
  reg->fn[user] = fn;
  reg->ctx[user] = ctx;
  if (!reg->active[user]) {
    reg->active[user] = true;
    reg->count++;
  }
  return ESP_OK;
}

esp_err_t radio_lease_registry_set_cb(radio_lease_registry_t* reg,
                                      uint8_t user,
                                      radio_lease_rx_fn fn,
                                      void* ctx) {
  if (reg == NULL || fn == NULL || user >= RADIO_LEASE_MAX_USERS) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!reg->active[user]) {
    return ESP_ERR_NOT_FOUND;
  }
  reg->fn[user] = fn;
  reg->ctx[user] = ctx;
  return ESP_OK;
}

esp_err_t radio_lease_registry_release(radio_lease_registry_t* reg,
                                       uint8_t user) {
  if (reg == NULL || user >= RADIO_LEASE_MAX_USERS) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!reg->active[user]) {
    return ESP_ERR_NOT_FOUND;
  }
  reg->fn[user] = NULL;
  reg->ctx[user] = NULL;
  reg->active[user] = false;
  if (reg->count > 0) {
    reg->count--;
  }
  return ESP_OK;
}

uint8_t radio_lease_registry_count(const radio_lease_registry_t* reg) {
  return (reg == NULL) ? 0 : reg->count;
}

bool radio_lease_registry_holds(const radio_lease_registry_t* reg,
                                uint8_t user) {
  if (reg == NULL || user >= RADIO_LEASE_MAX_USERS) {
    return false;
  }
  return reg->active[user];
}

uint8_t radio_lease_registry_dispatch(radio_lease_registry_t* reg,
                                      const void* buf,
                                      int type) {
  if (reg == NULL) {
    return 0;
  }

  /* Snapshot under a critical section, call outside it. The WiFi promiscuous
     callback already runs on the WiFi task, and a consumer is free to start or
     stop from inside its own handler (the analyzer does, when it hits its
     packet limit). */
  radio_lease_rx_fn fn[RADIO_LEASE_MAX_USERS];
  void* ctx[RADIO_LEASE_MAX_USERS];
  uint8_t n = 0;
  portENTER_CRITICAL(&s_mux);
  for (uint8_t i = 0; i < RADIO_LEASE_MAX_USERS && n < RADIO_LEASE_MAX_USERS;
       i++) {
    if (reg->active[i] && reg->fn[i] != NULL) {
      fn[n] = reg->fn[i];
      ctx[n] = reg->ctx[i];
      n++;
    }
  }
  portEXIT_CRITICAL(&s_mux);

  for (uint8_t i = 0; i < n; i++) {
    fn[i](ctx[i], buf, type);
  }
  return n;
}

bool radio_lease_stack_allowed(uint8_t wifi_refs,
                               uint8_t ble_refs,
                               radio_stack_t requested) {
  if (requested == RADIO_STACK_WIFI) {
    return ble_refs == 0;
  }
  return wifi_refs == 0;
}

uint8_t radio_lease_ref_acquire(uint8_t* count) {
  if (count == NULL) {
    return 0;
  }
  if (*count < 0xFF) {
    (*count)++;
  }
  return *count;
}

uint8_t radio_lease_ref_release(uint8_t* count) {
  if (count == NULL || *count == 0) {
    return 0;
  }
  (*count)--;
  return *count;
}
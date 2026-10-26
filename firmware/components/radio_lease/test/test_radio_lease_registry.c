/* Host tests for the shared-radio registry.
#include <stdint.h>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why these exist
 * ---------------
 * The ESP32-C6 has one 2.4 GHz radio, and ESP-IDF keeps exactly one WiFi
 * promiscuous callback plus one on/off switch. Six subsystems need it: the WiFi
 * analyzer/sniffer, the deauth detector, the DroneID scanner, the RF spectrum
 * analyzer, the SDR receiver and the surveillance detector.
 *
 * They used to call esp_wifi_set_promiscuous_rx_cb()/esp_wifi_set_promiscuous()
 * directly. That is a last-writer-wins global, and it produced two concrete
 * failures:
 *
 *   - the surveillance detector overwrote the deauth detector's packet_handler,
 *     so its deauth counter froze whenever the two overlapped;
 *   - esp_wifi_set_promiscuous(false) at the end of a detector window killed a
 *     capture the analyzer was relying on.
 *
 * This registry is the replacement. The tests pin the two properties the glue
 * layer depends on: frames reach *every* holder, and the "is anybody else using
 * this?" answer the glue layer gates its driver transitions on is accurate.
 */

#include <string.h>

#include "radio_lease_registry.h"
#include "unity.h"

/* Distinct receivers, one per consumer, so a fan-out can be attributed. */
static int g_rx[8];
static int g_type[8];
static int g_calls;

static void reset_rx(void) {
  for (int i = 0; i < 8; i++) {
    g_rx[i] = 0;
    g_type[i] = -1;
  }
  g_calls = 0;
}

static void receiver(void* ctx, const void* buf, int type) {
  (void) buf;
  int idx = (int) (intptr_t) ctx;
  g_calls++;
  if (idx >= 0 && idx < 8) {
    g_rx[idx]++;
    g_type[idx] = type;
  }
}

void setUp(void) {
  reset_rx();
}

void tearDown(void) {}

/* --- fan-out ----------------------------------------------------------- */

static void test_every_holder_receives_every_frame(void) {
  radio_lease_registry_t reg;
  radio_lease_registry_reset(&reg);

  TEST_ASSERT_EQUAL(ESP_OK, radio_lease_registry_acquire(&reg, 0, receiver,
                                                         (void*) (intptr_t) 0));
  TEST_ASSERT_EQUAL(ESP_OK, radio_lease_registry_acquire(&reg, 1, receiver,
                                                         (void*) (intptr_t) 1));
  TEST_ASSERT_EQUAL(ESP_OK, radio_lease_registry_acquire(&reg, 2, receiver,
                                                         (void*) (intptr_t) 2));

  TEST_ASSERT_EQUAL_UINT8(3, radio_lease_registry_dispatch(&reg, "f", 42));
  TEST_ASSERT_EQUAL_INT(3, g_calls);
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_EQUAL_INT(1, g_rx[i]);
    TEST_ASSERT_EQUAL_INT(42, g_type[i]);
  }

  radio_lease_registry_dispatch(&reg, "f", 43);
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_EQUAL_INT(2, g_rx[i]);
  }
}

static void test_releasing_one_consumer_keeps_the_others_receiving(void) {
  radio_lease_registry_t reg;
  radio_lease_registry_reset(&reg);
  radio_lease_registry_acquire(&reg, 0, receiver, (void*) (intptr_t) 0);
  radio_lease_registry_acquire(&reg, 1, receiver, (void*) (intptr_t) 1);

  TEST_ASSERT_EQUAL(ESP_OK, radio_lease_registry_release(&reg, 0));
  TEST_ASSERT_EQUAL_UINT8(1, radio_lease_registry_count(&reg));
  TEST_ASSERT_FALSE(radio_lease_registry_holds(&reg, 0));
  TEST_ASSERT_TRUE(radio_lease_registry_holds(&reg, 1));

  reset_rx();
  radio_lease_registry_dispatch(&reg, "f", 1);
  TEST_ASSERT_EQUAL_INT(0, g_rx[0]);
  TEST_ASSERT_EQUAL_INT(1, g_rx[1]);

  TEST_ASSERT_EQUAL(ESP_OK, radio_lease_registry_release(&reg, 1));
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_count(&reg));

  reset_rx();
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_dispatch(&reg, "f", 1));
  TEST_ASSERT_EQUAL_INT(0, g_calls);
}

/* --- the count is what the glue layer gates driver transitions on ------- */

static void test_count_is_only_zero_when_nobody_holds_it(void) {
  radio_lease_registry_t reg;
  radio_lease_registry_reset(&reg);
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_count(&reg));

  radio_lease_registry_acquire(&reg, 3, receiver, (void*) (intptr_t) 3);
  TEST_ASSERT_EQUAL_UINT8(1, radio_lease_registry_count(&reg));

  /* A second consumer must keep the count above zero - this is exactly the
     value the glue layer compares against before switching the driver off. */
  radio_lease_registry_acquire(&reg, 4, receiver, (void*) (intptr_t) 4);
  TEST_ASSERT_EQUAL_UINT8(2, radio_lease_registry_count(&reg));

  radio_lease_registry_release(&reg, 3);
  TEST_ASSERT_GREATER_THAN(0, radio_lease_registry_count(&reg));
  radio_lease_registry_release(&reg, 4);
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_count(&reg));
}

static void test_reacquiring_does_not_inflate_the_count(void) {
  /* The WiFi sniffer registers its callback on every start; that must update
     the receiver, not add a second holder, or the count would drift upwards and
     the driver would never be released again. */
  radio_lease_registry_t reg;
  radio_lease_registry_reset(&reg);

  radio_lease_registry_acquire(&reg, 1, receiver, (void*) (intptr_t) 1);
  TEST_ASSERT_EQUAL_UINT8(1, radio_lease_registry_count(&reg));

  radio_lease_registry_acquire(&reg, 1, receiver, (void*) (intptr_t) 5);
  TEST_ASSERT_EQUAL_UINT8(1, radio_lease_registry_count(&reg));

  /* And the receiver was swapped: the frame now goes to the new one. */
  reset_rx();
  radio_lease_registry_dispatch(&reg, "f", 1);
  TEST_ASSERT_EQUAL_INT(0, g_rx[1]);
  TEST_ASSERT_EQUAL_INT(1, g_rx[5]);

  TEST_ASSERT_EQUAL(ESP_OK, radio_lease_registry_release(&reg, 1));
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_count(&reg));
}

/* --- error paths ------------------------------------------------------- */

static void test_invalid_arguments_are_rejected(void) {
  radio_lease_registry_t reg;
  radio_lease_registry_reset(&reg);

  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                    radio_lease_registry_acquire(&reg, RADIO_LEASE_MAX_USERS,
                                                 receiver, NULL));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                    radio_lease_registry_acquire(&reg, 0, NULL, NULL));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                    radio_lease_registry_acquire(NULL, 0, receiver, NULL));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                    radio_lease_registry_release(&reg, RADIO_LEASE_MAX_USERS));
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, radio_lease_registry_release(&reg, 2));
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_count(&reg));
}

static void test_every_slot_can_be_held_simultaneously(void) {
  /* The consumer id is its slot, so all of them can hold at once and none can
     displace another - the failure mode the registry exists to remove. */
  radio_lease_registry_t reg;
  radio_lease_registry_reset(&reg);

  /* Slot 7 is deliberately left free so the re-acquire below has a receiver
     index of its own to observe. */
  for (uint8_t i = 0; i + 1 < RADIO_LEASE_MAX_USERS; i++) {
    TEST_ASSERT_EQUAL(ESP_OK, radio_lease_registry_acquire(
                                  &reg, i, receiver, (void*) (intptr_t) i));
    TEST_ASSERT_TRUE(radio_lease_registry_holds(&reg, i));
  }
  TEST_ASSERT_FALSE(
      radio_lease_registry_holds(&reg, RADIO_LEASE_MAX_USERS - 1));

  /* Re-acquiring a held slot must not add a holder or drop anybody. */
  TEST_ASSERT_EQUAL(ESP_OK, radio_lease_registry_acquire(&reg, 2, receiver,
                                                         (void*) (intptr_t) 7));
  TEST_ASSERT_EQUAL_UINT8(RADIO_LEASE_MAX_USERS - 1,
                          radio_lease_registry_count(&reg));

  TEST_ASSERT_EQUAL_UINT8(RADIO_LEASE_MAX_USERS - 1,
                          radio_lease_registry_dispatch(&reg, "f", 1));
  /* Slot 2 now reports under its new context and not under the old one. */
  TEST_ASSERT_EQUAL_INT(0, g_rx[2]);
  TEST_ASSERT_EQUAL_INT(1, g_rx[7]);
}

static void test_null_registry_is_safe(void) {
  radio_lease_registry_reset(NULL);
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_count(NULL));
  TEST_ASSERT_FALSE(radio_lease_registry_holds(NULL, 0));
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_dispatch(NULL, "f", 0));
}

static void test_reset_drops_every_holder(void) {
  radio_lease_registry_t reg;
  radio_lease_registry_reset(&reg);
  radio_lease_registry_acquire(&reg, 0, receiver, (void*) (intptr_t) 0);
  radio_lease_registry_acquire(&reg, 5, receiver, (void*) (intptr_t) 5);
  TEST_ASSERT_EQUAL_UINT8(2, radio_lease_registry_count(&reg));

  radio_lease_registry_reset(&reg);
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_count(&reg));
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_registry_dispatch(&reg, "f", 0));
}

/* --- plain reference counters ------------------------------------------ */

static void test_reference_counter_acquire_and_release(void) {
  uint8_t n = 0;
  TEST_ASSERT_EQUAL_UINT8(1, radio_lease_ref_acquire(&n));
  TEST_ASSERT_EQUAL_UINT8(2, radio_lease_ref_acquire(&n));
  TEST_ASSERT_EQUAL_UINT8(1, radio_lease_ref_release(&n));
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_ref_release(&n));
}

static void test_reference_counter_never_underflows(void) {
  uint8_t n = 0;
  /* An extra release (a stop path that runs twice) must not wrap to 255, which
     would make every later acquire look like the first one and leave the driver
     permanently held. */
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_ref_release(&n));
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_ref_release(&n));
  TEST_ASSERT_EQUAL_UINT8(1, radio_lease_ref_acquire(&n));
}

static void test_reference_counter_saturates(void) {
  uint8_t n = 250;
  for (int i = 0; i < 20; i++) {
    radio_lease_ref_acquire(&n);
  }
  TEST_ASSERT_EQUAL_UINT8(255, n);
}

static void test_wifi_and_ble_are_mutually_exclusive(void) {
  TEST_ASSERT_TRUE(radio_lease_stack_allowed(0, 0, RADIO_STACK_WIFI));
  TEST_ASSERT_TRUE(radio_lease_stack_allowed(0, 0, RADIO_STACK_BLE));
  TEST_ASSERT_TRUE(radio_lease_stack_allowed(3, 0, RADIO_STACK_WIFI));
  TEST_ASSERT_TRUE(radio_lease_stack_allowed(0, 2, RADIO_STACK_BLE));
  TEST_ASSERT_FALSE(radio_lease_stack_allowed(1, 0, RADIO_STACK_BLE));
  TEST_ASSERT_FALSE(radio_lease_stack_allowed(0, 1, RADIO_STACK_WIFI));
}

static void test_reference_counter_null_is_safe(void) {
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_ref_acquire(NULL));
  TEST_ASSERT_EQUAL_UINT8(0, radio_lease_ref_release(NULL));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_every_holder_receives_every_frame);
  RUN_TEST(test_releasing_one_consumer_keeps_the_others_receiving);
  RUN_TEST(test_count_is_only_zero_when_nobody_holds_it);
  RUN_TEST(test_reacquiring_does_not_inflate_the_count);
  RUN_TEST(test_invalid_arguments_are_rejected);
  RUN_TEST(test_every_slot_can_be_held_simultaneously);
  RUN_TEST(test_null_registry_is_safe);
  RUN_TEST(test_reset_drops_every_holder);
  RUN_TEST(test_reference_counter_acquire_and_release);
  RUN_TEST(test_reference_counter_never_underflows);
  RUN_TEST(test_reference_counter_saturates);
  RUN_TEST(test_wifi_and_ble_are_mutually_exclusive);
  RUN_TEST(test_reference_counter_null_is_safe);
  return UNITY_END();
}
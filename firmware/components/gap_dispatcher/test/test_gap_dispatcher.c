/* Host tests for gap_dispatcher.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why these exist
 * ---------------
 * Bluedroid stores exactly one GAP callback: every call to
 * esp_ble_gap_register_callback() overwrites the previous one, so whichever
 * module registered last silently starves the others. gap_dispatcher exists to
 * prevent that, and several apps (BLE spam, BLE HID, the trackers/ADV scanners,
 * the Flock detector and the gatt console commands) depend on it being right.
 *
 * Two real regressions came out of this area:
 *   - BLE HID stopped registering at all, so nothing started advertising.
 *   - Four gattcmd handlers were "registered" through event_bus_subscribe()
 *     with an app-private topic, which event_bus_subscribe() rejects, so
 *     ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT was never handled and no scan
 *     ever started.
 *
 * These tests pin the fan-out contract: every subscriber sees every event, no
 * module can take the slot away from the others, and the subscriber table is
 * bounded rather than silently overwriting an existing entry.
 */

#include <string.h>

#include "esp_bt_main.h"
#include "gap_dispatcher.h"
#include "unity.h"

/* One counter per subscriber so a fan-out can be checked per handler. */
static int s_calls_a;
static int s_calls_b;
static int s_calls_c;
static int s_calls_filler;
static esp_gap_ble_cb_event_t s_last_event;

static void cb_a(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
  (void)param;
  s_calls_a++;
  s_last_event = event;
}

static void cb_b(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
  (void)param;
  s_calls_b++;
  s_last_event = event;
}

static void cb_c(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
  (void)param;
  s_calls_c++;
  s_last_event = event;
}

/* Deliver an event the way Bluedroid would: through the single handler that
   gap_dispatcher installed. */
static void fire(esp_gap_ble_cb_event_t event) {
  esp_gap_ble_cb_t handler = gd_test_installed_handler();
  TEST_ASSERT_NOT_NULL_MESSAGE(handler,
                               "gap_dispatcher must own the Bluedroid slot");
  handler(event, NULL);
}

void setUp(void) {
  s_calls_a = s_calls_b = s_calls_c = s_calls_filler = 0;
  s_last_event = ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT;

  /* Drive a full disable -> wipe -> enable cycle so every test starts from a
     known state: this is exactly what a Bluedroid restart looks like to the
     dispatcher, and it is the only thing that makes it re-register. */
  gap_dispatcher_init();
  gd_test_set_bluedroid_status(ESP_BLUEDROID_STATUS_UNINITIALIZED);
  gap_dispatcher_init();
  gd_test_reset();
  gd_test_set_bluedroid_status(ESP_BLUEDROID_STATUS_ENABLED);
  gap_dispatcher_init();
}

void tearDown(void) {
  /* Drop every subscriber so tests cannot leak into each other. */
  gap_dispatcher_unregister(cb_a);
  gap_dispatcher_unregister(cb_b);
  gap_dispatcher_unregister(cb_c);
}

/* --- the contract ------------------------------------------------------ */

static void test_register_installs_the_dispatcher_as_the_bluedroid_handler(void) {
  TEST_ASSERT_NOT_NULL(gd_test_installed_handler());
}

static void test_every_subscriber_sees_every_event(void) {
  TEST_ASSERT_EQUAL(ESP_OK, gap_dispatcher_register(cb_a));
  TEST_ASSERT_EQUAL(ESP_OK, gap_dispatcher_register(cb_b));
  TEST_ASSERT_EQUAL(ESP_OK, gap_dispatcher_register(cb_c));

  fire(ESP_GAP_BLE_ADV_START_COMPLETE_EVT);
  TEST_ASSERT_EQUAL_INT(1, s_calls_a);
  TEST_ASSERT_EQUAL_INT(1, s_calls_b);
  TEST_ASSERT_EQUAL_INT(1, s_calls_c);

  fire(ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT);
  TEST_ASSERT_EQUAL_INT(2, s_calls_a);
  TEST_ASSERT_EQUAL_INT(2, s_calls_b);
  TEST_ASSERT_EQUAL_INT(2, s_calls_c);
  TEST_ASSERT_EQUAL(ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, s_last_event);
}

static void test_unregister_removes_only_that_subscriber(void) {
  gap_dispatcher_register(cb_a);
  gap_dispatcher_register(cb_b);
  TEST_ASSERT_EQUAL(ESP_OK, gap_dispatcher_unregister(cb_a));

  fire(ESP_GAP_BLE_ADV_START_COMPLETE_EVT);
  TEST_ASSERT_EQUAL_INT(0, s_calls_a);
  TEST_ASSERT_EQUAL_INT(1, s_calls_b);
}

static void test_registering_twice_does_not_duplicate(void) {
  /* BLE spam and the scanners re-enter their begin() on every menu visit. */
  TEST_ASSERT_EQUAL(ESP_OK, gap_dispatcher_register(cb_a));
  TEST_ASSERT_EQUAL(ESP_OK, gap_dispatcher_register(cb_a));

  fire(ESP_GAP_BLE_ADV_START_COMPLETE_EVT);
  TEST_ASSERT_EQUAL_INT(1, s_calls_a);
}

static void test_registering_later_does_not_starve_earlier_subscribers(void) {
  /* The failure this whole component exists to prevent: a second module
     calling esp_ble_gap_register_callback() directly and taking the slot. */
  gap_dispatcher_register(cb_a);
  esp_gap_ble_cb_t handler = gd_test_installed_handler();
  TEST_ASSERT_NOT_NULL(handler);

  /* register() must not swap the Bluedroid slot for the new subscriber. */
  TEST_ASSERT_EQUAL(ESP_OK, gap_dispatcher_register(cb_b));
  TEST_ASSERT_EQUAL_PTR((void*)handler, (void*)gd_test_installed_handler());

  fire(ESP_GAP_BLE_ADV_START_COMPLETE_EVT);
  TEST_ASSERT_EQUAL_INT(1, s_calls_a);
  TEST_ASSERT_EQUAL_INT(1, s_calls_b);
}

static void test_registering_after_a_bluedroid_restart_reclaims_the_slot(void) {
  /* surv_radio disables and re-enables Bluedroid, which drops the stack's
     handler. The next registration has to put it back or nobody gets events. */
  gap_dispatcher_register(cb_a);
  gap_dispatcher_unregister(cb_a);

  /* Stand in for the disable/enable cycle surv_radio performs: the dispatcher
     observes the disable, the stack forgets its handler, then it comes back. */
  gd_test_set_bluedroid_status(ESP_BLUEDROID_STATUS_UNINITIALIZED);
  gap_dispatcher_init();
  gd_test_forget_handler();
  gd_test_set_bluedroid_status(ESP_BLUEDROID_STATUS_ENABLED);
  TEST_ASSERT_NULL(gd_test_installed_handler());

  TEST_ASSERT_EQUAL(ESP_OK, gap_dispatcher_register(cb_b));
  TEST_ASSERT_NOT_NULL_MESSAGE(gd_test_installed_handler(),
                               "the dispatcher must reclaim the Bluedroid slot");

  fire(ESP_GAP_BLE_ADV_START_COMPLETE_EVT);
  TEST_ASSERT_EQUAL_INT(1, s_calls_b);
}

/* --- error paths ------------------------------------------------------- */

static void test_null_callback_is_rejected(void) {
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, gap_dispatcher_register(NULL));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, gap_dispatcher_unregister(NULL));
}

static void test_unregistering_an_unknown_callback_reports_not_found(void) {
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, gap_dispatcher_unregister(cb_c));
}

/* GAP_SUBSCRIBER_MAX distinct handlers, so the table really does fill up. The
   dispatcher compares callback pointers, so they have to be distinct
   addresses; a macro keeps them from drifting into duplicates. */
#define FILLER(i)                                          \
  static void cb_filler_##i(esp_gap_ble_cb_event_t event,  \
                            esp_ble_gap_cb_param_t* param) { \
    (void)event;                                           \
    (void)param;                                           \
    s_calls_filler++;                                      \
  }
FILLER(0) FILLER(1) FILLER(2) FILLER(3) FILLER(4)
FILLER(5) FILLER(6) FILLER(7) FILLER(8) FILLER(9)
FILLER(10) FILLER(11) FILLER(12) FILLER(13) FILLER(14)
FILLER(15) FILLER(16) FILLER(17) FILLER(18) FILLER(19)
FILLER(20) FILLER(21) FILLER(22) FILLER(23) FILLER(24)

static const esp_gap_ble_cb_t s_fillers[] = {
    cb_filler_0,  cb_filler_1,  cb_filler_2,  cb_filler_3,  cb_filler_4,
    cb_filler_5,  cb_filler_6,  cb_filler_7,  cb_filler_8,  cb_filler_9,
    cb_filler_10, cb_filler_11, cb_filler_12, cb_filler_13, cb_filler_14,
    cb_filler_15, cb_filler_16, cb_filler_17, cb_filler_18, cb_filler_19,
    cb_filler_20, cb_filler_21, cb_filler_22, cb_filler_23, cb_filler_24,
};

static void test_subscriber_table_is_bounded_and_reports_overflow(void) {
  /* Register one known subscriber first, so we can prove an overflowing
     registration does not evict it. */
  TEST_ASSERT_EQUAL(ESP_OK, gap_dispatcher_register(cb_a));

  /* The table then accepts distinct handlers up to GAP_SUBSCRIBER_MAX and
     refuses the rest instead of overwriting one of them. */
  unsigned accepted = 0;
  unsigned refused = 0;
  for (unsigned i = 0; i < sizeof(s_fillers) / sizeof(s_fillers[0]); i++) {
    esp_err_t ret = gap_dispatcher_register(s_fillers[i]);
    if (ret == ESP_OK) {
      accepted++;
    } else {
      TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, ret);
      refused++;
    }
  }
  TEST_ASSERT_GREATER_THAN(0, accepted);
  TEST_ASSERT_GREATER_THAN(0, refused);
  /* Refusing new subscribers must not have dropped the earlier ones. */
  fire(ESP_GAP_BLE_ADV_START_COMPLETE_EVT);
  TEST_ASSERT_EQUAL_INT((int)accepted, s_calls_filler);
  TEST_ASSERT_EQUAL_INT(1, s_calls_a);

  for (unsigned i = 0; i < sizeof(s_fillers) / sizeof(s_fillers[0]); i++) {
    gap_dispatcher_unregister(s_fillers[i]);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_register_installs_the_dispatcher_as_the_bluedroid_handler);
  RUN_TEST(test_every_subscriber_sees_every_event);
  RUN_TEST(test_unregister_removes_only_that_subscriber);
  RUN_TEST(test_registering_twice_does_not_duplicate);
  RUN_TEST(test_registering_later_does_not_starve_earlier_subscribers);
  RUN_TEST(test_registering_after_a_bluedroid_restart_reclaims_the_slot);
  RUN_TEST(test_null_callback_is_rejected);
  RUN_TEST(test_unregistering_an_unknown_callback_reports_not_found);
  RUN_TEST(test_subscriber_table_is_bounded_and_reports_overflow);
  return UNITY_END();
}
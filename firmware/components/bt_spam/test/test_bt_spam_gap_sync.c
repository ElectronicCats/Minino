/* Host tests for bt_spam_gap_sync.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why these exist
 * ---------------
 * The BLE spam app synchronises its advertising cycle against Bluedroid GAP
 * completion events, all of which arrive on a *single* binary semaphore:
 *
 *   stop -> SET_STATIC_RAND_ADDR -> ADV_DATA_RAW_SET -> ADV_START
 *
 * An earlier version of wait_gap_event() returned success for whichever token
 * woke it first. Because bt_spam_app_main() also issued a stop and drained the
 * semaphore *after* issuing it (too early for the async completion to have been
 * delivered), one stale token was always sitting in the semaphore when the adv
 * loop started. Every wait was then off by one: the loop believed advertising
 * was active when the controller had never confirmed the start, and the spam
 * stopped actually spamming.
 *
 * These tests pin the decision the loop makes on each wake-up. They model the
 * semaphore with `completed`/`mark` instead of a real FreeRTOS primitive, which
 * is exactly what the production code keys off.
 */

#include <string.h>

#include "bt_spam_gap_sync.h"
#include "unity.h"

/* --- model of the one-cycle sequence the adv loop performs --------------- */

typedef struct {
  bt_spam_gap_sync_t sync;
  /* Tokens currently sitting in the semaphore. */
  uint32_t tokens;
} fake_sem_t;

static void fake_init(fake_sem_t* f) {
  bt_spam_gap_sync_reset(&f->sync);
  f->tokens = 0;
}

/* The GAP callback: record the completion and post a token. */
static void fake_gap_event(fake_sem_t* f, esp_gap_ble_cb_event_t event,
                           esp_bt_status_t status) {
  bt_spam_gap_sync_complete(&f->sync, event, status);
  f->tokens++;
}

/* drain_sem() */
static void fake_drain(fake_sem_t* f) {
  f->tokens = 0;
}

/*
 * Models one wait_gap_event() call.
 *
 * The ordering that matters: the snapshot is taken *after* drain_sem() and
 * *before* the command is issued, so the completion always arrives while the
 * wait is already in progress. Events queued by `fake_defer` are delivered
 * first, which is how the "an unrelated completion got there first" case is
 * built.
 */
#define FAKE_MAX_DEFERRED 4

typedef struct {
  esp_gap_ble_cb_event_t event[FAKE_MAX_DEFERRED];
  esp_bt_status_t status[FAKE_MAX_DEFERRED];
  unsigned count;
} deferred_t;

static void fake_defer(deferred_t* d, esp_gap_ble_cb_event_t event,
                       esp_bt_status_t status) {
  d->event[d->count] = event;
  d->status[d->count] = status;
  d->count++;
}

/* Snapshot, as wait_gap_event() does before touching the semaphore. */
static uint32_t fake_begin_wait(const fake_sem_t* f) {
  return bt_spam_gap_sync_mark(&f->sync);
}

/* Deliver the deferred events plus the awaited one, then resolve. */
static bool fake_pump(fake_sem_t* f, const deferred_t* d,
                      esp_gap_ble_cb_event_t wanted) {
  uint32_t mark = fake_begin_wait(f);
  for (unsigned i = 0; i < d->count; i++) {
    fake_gap_event(f, d->event[i], d->status[i]);
  }
  fake_gap_event(f, wanted, ESP_BT_STATUS_SUCCESS);

  for (int i = 0; i < 8; i++) { /* bounded so a broken matcher terminates */
    if (!bt_spam_gap_sync_has_new(&f->sync, mark)) {
      if (f->tokens == 0) {
        return false; /* timed out */
      }
      f->tokens--;
      continue;
    }
    mark = bt_spam_gap_sync_mark(&f->sync);
    if (bt_spam_gap_sync_matches(&f->sync, wanted)) {
      return bt_spam_gap_sync_succeeded(&f->sync);
    }
  }
  return false;
}

/* Nothing arrives at all: the wait times out. */
static bool fake_pump_silent(fake_sem_t* f, esp_gap_ble_cb_event_t wanted) {
  (void)wanted;
  uint32_t mark = fake_begin_wait(f);
  return bt_spam_gap_sync_has_new(&f->sync, mark);
}

void setUp(void) {}
void tearDown(void) {}

/* --- basics ------------------------------------------------------------ */

static void test_reset_clears_state(void) {
  fake_sem_t f;
  fake_init(&f);
  fake_gap_event(&f, ESP_GAP_BLE_ADV_START_COMPLETE_EVT, ESP_BT_STATUS_FAIL);
  TEST_ASSERT_GREATER_THAN(0, f.sync.completed);

  bt_spam_gap_sync_reset(&f.sync);
  TEST_ASSERT_EQUAL_UINT32(0, f.sync.completed);
  TEST_ASSERT_TRUE(bt_spam_gap_sync_succeeded(&f.sync));
}

static void test_null_pointers_are_survivable(void) {
  bt_spam_gap_sync_reset(NULL);
  TEST_ASSERT_FALSE(bt_spam_gap_sync_complete(NULL,
                                             ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT,
                                             ESP_BT_STATUS_SUCCESS));
  TEST_ASSERT_EQUAL_UINT32(0, bt_spam_gap_sync_mark(NULL));
  TEST_ASSERT_FALSE(bt_spam_gap_sync_has_new(NULL, 0));
  TEST_ASSERT_FALSE(
      bt_spam_gap_sync_matches(NULL, ESP_GAP_BLE_ADV_START_COMPLETE_EVT));
  TEST_ASSERT_FALSE(bt_spam_gap_sync_succeeded(NULL));
}

/* --- the regression ---------------------------------------------------- */

static void test_stale_token_does_not_answer_the_next_request(void) {
  /* bt_spam_app_main() leaves a leftover ADV_STOP completion in the
     semaphore. The first wait of the adv loop asks for SET_STATIC_RAND_ADDR
     and must not be satisfied by it. */
  fake_sem_t f;
  fake_init(&f);

  fake_gap_event(&f, ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, ESP_BT_STATUS_SUCCESS);
  fake_drain(&f); /* drain_sem() runs before the stop ... */
  fake_gap_event(&f, ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, ESP_BT_STATUS_SUCCESS);
  fake_drain(&f); /* ... and again after it, before the loop starts */

  uint32_t mark = bt_spam_gap_sync_mark(&f.sync);
  TEST_ASSERT_FALSE_MESSAGE(bt_spam_gap_sync_has_new(&f.sync, mark),
                            "a stale completion must not look new after a drain");
  TEST_ASSERT_FALSE_MESSAGE(
      bt_spam_gap_sync_matches(&f.sync, ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT),
      "a stale ADV_STOP must never match a SET_STATIC_RAND_ADDR request");
}

static void test_completion_before_the_mark_is_not_new(void) {
  fake_sem_t f;
  fake_init(&f);
  fake_gap_event(&f, ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT,
                 ESP_BT_STATUS_SUCCESS);

  /* Snapshot taken *after* the completion, as wait_gap_event() does. */
  uint32_t mark = bt_spam_gap_sync_mark(&f.sync);
  TEST_ASSERT_FALSE(bt_spam_gap_sync_has_new(&f.sync, mark));

  fake_drain(&f);
  fake_gap_event(&f, ESP_GAP_BLE_ADV_START_COMPLETE_EVT, ESP_BT_STATUS_SUCCESS);
  TEST_ASSERT_TRUE(bt_spam_gap_sync_has_new(&f.sync, mark));
  TEST_ASSERT_TRUE(
      bt_spam_gap_sync_matches(&f.sync, ESP_GAP_BLE_ADV_START_COMPLETE_EVT));
}

static void test_wrong_event_does_not_satisfy_a_wait(void) {
  fake_sem_t f;
  fake_init(&f);

  /* A completion for a different request shows up first. */
  fake_drain(&f);
  fake_gap_event(&f, ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, ESP_BT_STATUS_SUCCESS);
  uint32_t mark = bt_spam_gap_sync_mark(&f.sync);
  fake_gap_event(&f, ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT,
                 ESP_BT_STATUS_SUCCESS);

  TEST_ASSERT_TRUE(bt_spam_gap_sync_has_new(&f.sync, mark));
  TEST_ASSERT_TRUE(
      bt_spam_gap_sync_matches(&f.sync, ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT));
  TEST_ASSERT_FALSE(bt_spam_gap_sync_matches(&f.sync, ESP_GAP_BLE_ADV_START_COMPLETE_EVT));
}

/* --- full cycle -------------------------------------------------------- */

static void test_full_cycle_never_accepts_the_previous_cycle_event(void) {
  /* Replay two complete advertising cycles. Each request must be answered only
     by its own completion, which is what keeps is_advertising_active honest. */
  fake_sem_t f;
  fake_init(&f);

  deferred_t none = {0};

  for (int cycle = 0; cycle < 2; cycle++) {
    if (cycle > 0) {
      /* The loop stops advertising at the top of every cycle. */
      fake_drain(&f);
      TEST_ASSERT_TRUE_MESSAGE(
          fake_pump(&f, &none, ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT),
          "stop request must be confirmed by ADV_STOP");
    }

    fake_drain(&f);
    TEST_ASSERT_TRUE_MESSAGE(
        fake_pump(&f, &none, ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT),
        "MAC rotation must be confirmed by SET_STATIC_RAND_ADDR");

    fake_drain(&f);
    TEST_ASSERT_TRUE_MESSAGE(
        fake_pump(&f, &none, ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT),
        "payload config must be confirmed by ADV_DATA_RAW_SET");

    fake_drain(&f);
    TEST_ASSERT_TRUE_MESSAGE(fake_pump(&f, &none, ESP_GAP_BLE_ADV_START_COMPLETE_EVT),
                              "start request must be confirmed by ADV_START");
  }
}

static void test_out_of_order_completions_still_resolve_correctly(void) {
  /* A completion for an earlier request can land while a later request is
     being awaited (Bluedroid answers off BTC_TASK, not in issue order). The
     wait must skip it and keep going rather than return its status. */
  fake_sem_t f;
  fake_init(&f);
  fake_drain(&f);

  deferred_t late = {0};
  fake_defer(&late, ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, ESP_BT_STATUS_SUCCESS);
  fake_defer(&late, ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT, ESP_BT_STATUS_FAIL);
  TEST_ASSERT_TRUE_MESSAGE(fake_pump(&f, &late, ESP_GAP_BLE_ADV_START_COMPLETE_EVT),
                            "start must resolve on ADV_START, not on the two "
                            "completions that arrived before it");
}

static void test_every_out_of_order_permutation_resolves(void) {
  static const esp_gap_ble_cb_event_t order[] = {
      ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT,
      ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT,
      ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT,
  };
  static const esp_gap_ble_cb_event_t targets[] = {
      ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT,
      ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT,
      ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT,
      ESP_GAP_BLE_ADV_START_COMPLETE_EVT,
  };
  const unsigned n = sizeof(order) / sizeof(order[0]);

  for (unsigned t = 0; t < sizeof(targets) / sizeof(targets[0]); t++) {
    for (unsigned mask = 0; mask < (1u << n); mask++) {
      fake_sem_t f;
      fake_init(&f);
      fake_drain(&f);

      deferred_t d = {0};
      for (unsigned i = 0; i < n; i++) {
        if (mask & (1u << i)) {
          fake_defer(&d, order[i], ESP_BT_STATUS_FAIL);
        }
      }
      TEST_ASSERT_TRUE_MESSAGE(fake_pump(&f, &d, targets[t]),
                                "a preceding completion must never decide the "
                                "outcome of an unrelated request");
    }
  }
}

static void test_off_by_one_would_be_caught(void) {
  /* Guards the harness itself: if the matcher accepted any completion, the
     full-cycle test above would still pass, so assert explicitly that a wrong
     event is rejected. */
  fake_sem_t f;
  fake_init(&f);
  fake_drain(&f);
  fake_gap_event(&f, ESP_GAP_BLE_ADV_START_COMPLETE_EVT, ESP_BT_STATUS_SUCCESS);
  TEST_ASSERT_FALSE(bt_spam_gap_sync_matches(&f.sync,
                                             ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT));
  TEST_ASSERT_FALSE(
      bt_spam_gap_sync_matches(&f.sync, ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT));
  TEST_ASSERT_FALSE(
      bt_spam_gap_sync_matches(&f.sync, ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT));
}

/* --- failure propagation ------------------------------------------------ */

static void test_failure_status_is_reported(void) {
  fake_sem_t f;
  fake_init(&f);

  fake_drain(&f);
  uint32_t mark = fake_begin_wait(&f);
  fake_gap_event(&f, ESP_GAP_BLE_ADV_START_COMPLETE_EVT,
                 ESP_BT_STATUS_INVALID_STATE);
  TEST_ASSERT_TRUE(bt_spam_gap_sync_has_new(&f.sync, mark));
  TEST_ASSERT_TRUE(
      bt_spam_gap_sync_matches(&f.sync, ESP_GAP_BLE_ADV_START_COMPLETE_EVT));
  TEST_ASSERT_FALSE_MESSAGE(bt_spam_gap_sync_succeeded(&f.sync),
                            "a failed start must not be reported as success");
}

static void test_every_status_value_is_propagated(void) {
  const esp_bt_status_t failures[] = {
      ESP_BT_STATUS_FAIL, ESP_BT_STATUS_PENDING, ESP_BT_STATUS_BUSY,
      ESP_BT_STATUS_INVALID_STATE,
  };
  for (unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
    fake_sem_t f;
    fake_init(&f);
    fake_drain(&f);
    uint32_t mark = fake_begin_wait(&f);
    fake_gap_event(&f, ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT, failures[i]);
    TEST_ASSERT_TRUE(bt_spam_gap_sync_has_new(&f.sync, mark));
    TEST_ASSERT_TRUE(bt_spam_gap_sync_matches(
        &f.sync, ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT));
    TEST_ASSERT_FALSE(bt_spam_gap_sync_succeeded(&f.sync));
  }
}

static void test_timeout_when_nothing_arrives(void) {
  fake_sem_t f;
  fake_init(&f);
  fake_drain(&f);
  /* Nothing was ever issued: the wait must time out, not succeed. */
  TEST_ASSERT_FALSE(fake_pump_silent(&f, ESP_GAP_BLE_ADV_START_COMPLETE_EVT));
}

static void test_completed_counter_advances_monotonically(void) {
  bt_spam_gap_sync_t s;
  bt_spam_gap_sync_reset(&s);
  TEST_ASSERT_EQUAL_UINT32(0, bt_spam_gap_sync_mark(&s));
  for (uint32_t i = 1; i <= 10; i++) {
    bt_spam_gap_sync_complete(&s, ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT,
                              ESP_BT_STATUS_SUCCESS);
    TEST_ASSERT_EQUAL_UINT32(i, bt_spam_gap_sync_mark(&s));
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_reset_clears_state);
  RUN_TEST(test_null_pointers_are_survivable);
  RUN_TEST(test_stale_token_does_not_answer_the_next_request);
  RUN_TEST(test_completion_before_the_mark_is_not_new);
  RUN_TEST(test_wrong_event_does_not_satisfy_a_wait);
  RUN_TEST(test_full_cycle_never_accepts_the_previous_cycle_event);
  RUN_TEST(test_out_of_order_completions_still_resolve_correctly);
  RUN_TEST(test_every_out_of_order_permutation_resolves);
  RUN_TEST(test_off_by_one_would_be_caught);
  RUN_TEST(test_failure_status_is_reported);
  RUN_TEST(test_every_status_value_is_propagated);
  RUN_TEST(test_timeout_when_nothing_arrives);
  RUN_TEST(test_completed_counter_advances_monotonically);
  return UNITY_END();
}
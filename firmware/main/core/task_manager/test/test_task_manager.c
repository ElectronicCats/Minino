/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host tests for the task registry: a task that ends itself must give its
 * slot back, or the registry fills up and every later create fails. */
#include "task_manager.h"
#include "unity.h"

TaskHandle_t mock_current_task = NULL;
TaskHandle_t mock_last_deleted = NULL;
int mock_delete_calls = 0;

static void noop(void* arg) {
  (void) arg;
}

static TaskHandle_t create(void) {
  TaskHandle_t h = NULL;
  TEST_ASSERT_EQUAL(ESP_OK, task_manager_create(noop, "t", TASK_STACK_SMALL,
                                                NULL, TASK_PRIORITY_LOW, &h));
  return h;
}

void setUp(void) {
  task_manager_deinit();
  TEST_ASSERT_EQUAL(ESP_OK, task_manager_init());
  mock_current_task = NULL;
  mock_last_deleted = NULL;
  mock_delete_calls = 0;
}

void tearDown(void) {}

static void test_self_delete_frees_the_slot(void) {
  /* Far more short-lived tasks than the registry holds (50). */
  for (int i = 0; i < 200; i++) {
    mock_current_task = create();
    TEST_ASSERT_EQUAL(ESP_OK, task_manager_delete(NULL));
    TEST_ASSERT_EQUAL_PTR(mock_current_task, mock_last_deleted);
  }
  TEST_ASSERT_EQUAL_UINT32(0, task_manager_get_count());
}

static void test_unregistered_caller_is_still_deleted(void) {
  /* A pthread or plain xTaskCreate task: not in the registry, but it must
     still die rather than return. */
  mock_current_task = (TaskHandle_t) 0xBEEF;
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, task_manager_delete(NULL));
  TEST_ASSERT_EQUAL_PTR((TaskHandle_t) 0xBEEF, mock_last_deleted);
}

static void test_stale_handle_is_left_alone(void) {
  TaskHandle_t h = create();
  mock_current_task = h;
  TEST_ASSERT_EQUAL(ESP_OK, task_manager_delete(NULL));
  mock_current_task = NULL;
  /* Someone deletes it again by its old handle: its TCB is gone, so
     vTaskDelete must not be called. */
  int calls = mock_delete_calls;
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, task_manager_delete(h));
  TEST_ASSERT_EQUAL(calls, mock_delete_calls);
}

static void test_delete_by_handle_keeps_others(void) {
  TaskHandle_t a = create();
  TaskHandle_t b = create();
  TEST_ASSERT_EQUAL(ESP_OK, task_manager_delete(a));
  TEST_ASSERT_EQUAL_PTR(a, mock_last_deleted);
  TEST_ASSERT_EQUAL_UINT32(1, task_manager_get_count());
  TEST_ASSERT_TRUE(task_manager_get_info_copy(b, &(task_info_t) {0}));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_self_delete_frees_the_slot);
  RUN_TEST(test_unregistered_caller_is_still_deleted);
  RUN_TEST(test_stale_handle_is_left_alone);
  RUN_TEST(test_delete_by_handle_keeps_others);
  return UNITY_END();
}

#include <stdbool.h>
#include <stdint.h>

#include "app_manager.h"
#include "menus_module.h"
#include "unity.h"

static int enter_count;
static int exit_count;
static int input_count;
static uint8_t last_button;
static uint8_t last_event;

static bool menus_in_app;
static input_callback_t menus_input;
static int default_input_count;
static int menu_return_count;

void setUp(void) {}
void tearDown(void) {}

static void on_enter(void) {
  enter_count++;
}

static void on_input(uint8_t button_name, uint8_t button_event) {
  input_count++;
  last_button = button_name;
  last_event = button_event;
}

static void on_exit(void) {
  exit_count++;
}

static const minino_app_t app = {
    .name = "test",
    .on_enter = on_enter,
    .on_input = on_input,
    .on_exit = on_exit,
};

static const minino_app_t bare_app = {.name = "bare"};

void menus_module_set_app_state(bool in_app, input_callback_t input_cb) {
  menus_in_app = in_app;
  menus_input = input_cb;
}

void menus_module_set_default_input(void) {
  default_input_count++;
}

void menus_module_return(void) {
  menus_module_set_default_input();
  menus_in_app = false;
  menu_return_count++;
}

static int menu_exit_app_count;

void menus_module_exit_app(void) {
  menus_in_app = false;
  menu_exit_app_count++;
}

bool menus_module_get_app_state(void) {
  return menus_in_app;
}

static void reset(void) {
  while (app_manager_is_active()) {
    app_manager_exit();
  }
  enter_count = 0;
  exit_count = 0;
  input_count = 0;
  last_button = 0;
  last_event = 0;
  menus_in_app = false;
  menus_input = NULL;
  default_input_count = 0;
  menu_return_count = 0;
  menu_exit_app_count = 0;
}

/* Apps entered from the menu table never call app_manager_enter(); their back
   button must still leave through the menu. */
static void test_exit_without_active_app_leaves_menu_app(void) {
  reset();
  menus_in_app = true;
  app_manager_exit();
  TEST_ASSERT_EQUAL_INT(1, menu_exit_app_count);
  TEST_ASSERT_EQUAL_INT(0, menu_return_count);
  app_manager_exit();
  TEST_ASSERT_EQUAL_INT(1, menu_exit_app_count);
}

static void test_exit_outside_any_app_is_a_no_op(void) {
  reset();
  app_manager_exit();
  TEST_ASSERT_EQUAL_INT(0, menu_exit_app_count);
  TEST_ASSERT_EQUAL_INT(0, menu_return_count);
}

static void test_enter_installs_input_and_runs_on_enter(void) {
  reset();
  app_manager_enter(&app);
  TEST_ASSERT_TRUE(app_manager_is_active());
  TEST_ASSERT_EQUAL_PTR(&app, app_manager_active());
  TEST_ASSERT_EQUAL_INT(1, enter_count);
  TEST_ASSERT_TRUE(menus_in_app);
  TEST_ASSERT_NOT_NULL(menus_input);
}

static void test_input_is_forwarded_to_the_active_app(void) {
  reset();
  app_manager_enter(&app);
  menus_input(7, 9);
  TEST_ASSERT_EQUAL_INT(1, input_count);
  TEST_ASSERT_EQUAL_UINT8(7, last_button);
  TEST_ASSERT_EQUAL_UINT8(9, last_event);
}

static void test_exit_clears_state_and_returns_to_menu(void) {
  reset();
  app_manager_enter(&app);
  app_manager_exit();
  TEST_ASSERT_FALSE(app_manager_is_active());
  TEST_ASSERT_EQUAL_INT(1, exit_count);
  TEST_ASSERT_EQUAL_INT(1, default_input_count);
  TEST_ASSERT_EQUAL_INT(1, menu_return_count);
}

static void test_double_exit_is_a_no_op(void) {
  reset();
  app_manager_enter(&app);
  app_manager_exit();
  app_manager_exit();
  TEST_ASSERT_EQUAL_INT(1, exit_count);
  TEST_ASSERT_EQUAL_INT(1, menu_return_count);
}

static void test_second_enter_is_rejected_while_active(void) {
  reset();
  app_manager_enter(&app);
  app_manager_enter(&bare_app);
  TEST_ASSERT_EQUAL_PTR(&app, app_manager_active());
  TEST_ASSERT_EQUAL_INT(1, enter_count);
}

static void test_app_without_callbacks_is_supported(void) {
  reset();
  app_manager_enter(&bare_app);
  TEST_ASSERT_TRUE(app_manager_is_active());
  menus_input(1, 2);
  app_manager_exit();
  TEST_ASSERT_FALSE(app_manager_is_active());
  TEST_ASSERT_EQUAL_INT(1, menu_return_count);
}

static void test_null_app_is_ignored(void) {
  reset();
  app_manager_enter(NULL);
  TEST_ASSERT_FALSE(app_manager_is_active());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_enter_installs_input_and_runs_on_enter);
  RUN_TEST(test_input_is_forwarded_to_the_active_app);
  RUN_TEST(test_exit_clears_state_and_returns_to_menu);
  RUN_TEST(test_double_exit_is_a_no_op);
  RUN_TEST(test_second_enter_is_rejected_while_active);
  RUN_TEST(test_app_without_callbacks_is_supported);
  RUN_TEST(test_null_app_is_ignored);
  RUN_TEST(test_exit_without_active_app_leaves_menu_app);
  RUN_TEST(test_exit_outside_any_app_is_a_no_op);
  return UNITY_END();
}

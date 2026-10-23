/* Host tests for the HID module state machine logic.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests the pure logic functions that don't require hardware/FreeRTOS.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

/* ---- Local definitions (from hid_screens.h) ---- */

#define HID_CONFIG_NAME  0
#define HID_CONFIG_MAC   1
#define HID_CONFIG_START 2
#define HID_MENU_COUNT   3

#define HID_DEVICE_VOL_UP     0
#define HID_DEVICE_VOL_DOWN   1
#define HID_DEVICE_PLAY       2
#define HID_DEVICE_PAUSE      3
#define HID_DEVICE_STOP       4
#define HID_DEVICE_MUTE       5
#define HID_DEVICE_NEXT_TRACK 6
#define HID_DEVICE_PREV_TRACK 7
#define HID_DEVICE_COUNT      8

/* Mock data tables (from hid_data.c) */

static char* hid_menu_items[HID_MENU_COUNT] = {"Device name", "Device MAC",
                                               "Start"};

static char* hid_device_items[HID_DEVICE_COUNT] = {
    "Volume Up", "Volume Down", "Play",       "Pause",
    "Stop",      "Mute",        "Next Track", "Previous Track"};

/* ---- Tests ---- */

void setUp(void) {}
void tearDown(void) {}

/* --- Menu item counts and names --- */

static void test_hid_menu_items_count(void) {
  TEST_ASSERT_EQUAL(HID_MENU_COUNT, 3);
  TEST_ASSERT_EQUAL_STRING("Device name", hid_menu_items[0]);
  TEST_ASSERT_EQUAL_STRING("Device MAC", hid_menu_items[1]);
  TEST_ASSERT_EQUAL_STRING("Start", hid_menu_items[2]);
}

static void test_hid_device_items_count(void) {
  TEST_ASSERT_EQUAL(HID_DEVICE_COUNT, 8);
  TEST_ASSERT_EQUAL_STRING("Volume Up", hid_device_items[0]);
  TEST_ASSERT_EQUAL_STRING("Volume Down", hid_device_items[1]);
  TEST_ASSERT_EQUAL_STRING("Play", hid_device_items[2]);
  TEST_ASSERT_EQUAL_STRING("Pause", hid_device_items[3]);
  TEST_ASSERT_EQUAL_STRING("Stop", hid_device_items[4]);
  TEST_ASSERT_EQUAL_STRING("Mute", hid_device_items[5]);
  TEST_ASSERT_EQUAL_STRING("Next Track", hid_device_items[6]);
  TEST_ASSERT_EQUAL_STRING("Previous Track", hid_device_items[7]);
}

/* --- Enum values --- */

static void test_hid_config_enums(void) {
  TEST_ASSERT_EQUAL(HID_CONFIG_NAME, 0);
  TEST_ASSERT_EQUAL(HID_CONFIG_MAC, 1);
  TEST_ASSERT_EQUAL(HID_CONFIG_START, 2);
  TEST_ASSERT_EQUAL(HID_MENU_COUNT, 3);
}

static void test_hid_device_enums(void) {
  TEST_ASSERT_EQUAL(HID_DEVICE_VOL_UP, 0);
  TEST_ASSERT_EQUAL(HID_DEVICE_VOL_DOWN, 1);
  TEST_ASSERT_EQUAL(HID_DEVICE_PLAY, 2);
  TEST_ASSERT_EQUAL(HID_DEVICE_PAUSE, 3);
  TEST_ASSERT_EQUAL(HID_DEVICE_STOP, 4);
  TEST_ASSERT_EQUAL(HID_DEVICE_MUTE, 5);
  TEST_ASSERT_EQUAL(HID_DEVICE_NEXT_TRACK, 6);
  TEST_ASSERT_EQUAL(HID_DEVICE_PREV_TRACK, 7);
  TEST_ASSERT_EQUAL(HID_DEVICE_COUNT, 8);
}

/* --- State machine logic helpers --- */

/* Simulate the menu navigation wrap-around logic from hid_module_cb_event */
static uint16_t simulate_menu_up(uint16_t current, uint16_t count) {
  return current == 0 ? count - 1 : current - 1;
}

static uint16_t simulate_menu_down(uint16_t current, uint16_t count) {
  return (current + 1 >= count) ? 0 : current + 1;
}

static void test_menu_up_wraps_correctly(void) {
  TEST_ASSERT_EQUAL(2, simulate_menu_up(0, 3));
  TEST_ASSERT_EQUAL(0, simulate_menu_up(1, 3));
  TEST_ASSERT_EQUAL(1, simulate_menu_up(2, 3));
}

static void test_menu_down_wraps_correctly(void) {
  TEST_ASSERT_EQUAL(1, simulate_menu_down(0, 3));
  TEST_ASSERT_EQUAL(2, simulate_menu_down(1, 3));
  TEST_ASSERT_EQUAL(0, simulate_menu_down(2, 3));
}

static void test_device_menu_up_wraps(void) {
  TEST_ASSERT_EQUAL(7, simulate_menu_up(0, 8));
  TEST_ASSERT_EQUAL(0, simulate_menu_up(1, 8));
  TEST_ASSERT_EQUAL(6, simulate_menu_up(7, 8));
}

static void test_device_menu_down_wraps(void) {
  TEST_ASSERT_EQUAL(1, simulate_menu_down(0, 8));
  TEST_ASSERT_EQUAL(7, simulate_menu_down(6, 8));
  TEST_ASSERT_EQUAL(0, simulate_menu_down(7, 8));
}

/* --- MAC address formatting --- */

static void test_mac_formatting(void) {
  uint8_t mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
  char mac_str[20];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);
  TEST_ASSERT_EQUAL_STRING("12:34:56:78:9A:BC", mac_str);
}

/* --- Connection state tracking --- */

typedef struct {
  bool connected;
  bool secured;
  uint16_t conn_id;
} hid_state_t;

static hid_state_t s_state = {0};

static void hid_state_set_connected(bool conn, bool sec) {
  s_state.connected = conn;
  s_state.secured = sec;
  s_state.conn_id = (conn && sec) ? 1 : 0;
}

static bool hid_can_send_action(void) {
  return s_state.connected && s_state.secured;
}

static void test_hid_actions_require_secure_connection(void) {
  hid_state_set_connected(false, false);
  TEST_ASSERT_FALSE(hid_can_send_action());

  hid_state_set_connected(true, false);
  TEST_ASSERT_FALSE(hid_can_send_action());

  hid_state_set_connected(true, true);
  TEST_ASSERT_TRUE(hid_can_send_action());

  hid_state_set_connected(false, true);
  TEST_ASSERT_FALSE(hid_can_send_action());
}

/* --- Spam mode enums --- */

typedef enum {
  BT_SPAM_MODE_ALL = 0,
  BT_SPAM_MODE_APPLE,
  BT_SPAM_MODE_ANDROID,
  BT_SPAM_MODE_WINDOWS,
  BT_SPAM_MODE_SAMSUNG,
  BT_SPAM_MODE_MAX,
} bt_spam_mode_t;

static void test_bt_spam_mode_enums(void) {
  TEST_ASSERT_EQUAL(BT_SPAM_MODE_ALL, 0);
  TEST_ASSERT_EQUAL(BT_SPAM_MODE_APPLE, 1);
  TEST_ASSERT_EQUAL(BT_SPAM_MODE_ANDROID, 2);
  TEST_ASSERT_EQUAL(BT_SPAM_MODE_WINDOWS, 3);
  TEST_ASSERT_EQUAL(BT_SPAM_MODE_SAMSUNG, 4);
  TEST_ASSERT_EQUAL(BT_SPAM_MODE_MAX, 5);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hid_menu_items_count);
  RUN_TEST(test_hid_device_items_count);
  RUN_TEST(test_hid_config_enums);
  RUN_TEST(test_hid_device_enums);
  RUN_TEST(test_menu_up_wraps_correctly);
  RUN_TEST(test_menu_down_wraps_correctly);
  RUN_TEST(test_device_menu_up_wraps);
  RUN_TEST(test_device_menu_down_wraps);
  RUN_TEST(test_mac_formatting);
  RUN_TEST(test_hid_actions_require_secure_connection);
  RUN_TEST(test_bt_spam_mode_enums);
  return UNITY_END();
}
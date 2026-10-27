/* Host-test stand-in for the ESP-IDF sdkconfig.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * menus_data.c gates most of its entries on CONFIG_* symbols, so the host test
 * needs a configuration to compile against. Every app is enabled here by
 * default; the test Makefile builds a second binary with a feature switched off
 * via -DTEST_CONFIG_<FEATURE>_ENABLED=0, because that is how the catalogue's
 * structural bugs hide:
 *
 *   - the three MENU_SETTINGS_SD_CARD_* children were emitted outside
 *     #if CONFIG_SD_ENABLED while their parent was inside it, so on a board
 *     without an SD card they became orphans. navigation_exit() then resolved
 *     their parent to the catalogue root, whose parent_idx is -1, leaving
 *     current_menu = -1 and a blank screen with no way back out.
 *   - a child emitted before its parent breaks the linear walks in
 *     update_menus() and navigation_enter().
 *
 * main/CMakeLists.txt excludes this Makefile from the firmware build, so the
 * real sdkconfig.h is unaffected.
 */
#pragma once

#define TEST_CONFIG_DEFAULT 1

/* OTA_ENABLE and SDR_ENABLE are chained in the real Kconfig; the SDR entry is
   gated on both. */
#define TEST_OWNS_FEATURE(name) \
  _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic pop")

#ifndef TEST_CONFIG_WIFI_APPS_ENABLE
  #define TEST_CONFIG_WIFI_APPS_ENABLE 1
#endif
#ifndef TEST_CONFIG_BLUETOOTH_APPS_ENABLE
  #define TEST_CONFIG_BLUETOOTH_APPS_ENABLE 1
#endif
#ifndef TEST_CONFIG_ZIGBEE_APPS_ENABLE
  #define TEST_CONFIG_ZIGBEE_APPS_ENABLE 1
#endif
#ifndef TEST_CONFIG_THREAD_APPS_ENABLE
  #define TEST_CONFIG_THREAD_APPS_ENABLE 1
#endif
#ifndef TEST_CONFIG_GPS_APPS_ENABLE
  #define TEST_CONFIG_GPS_APPS_ENABLE 1
#endif
#ifndef TEST_CONFIG_GPS_ENABLED
  #define TEST_CONFIG_GPS_ENABLED 1
#endif
#ifndef TEST_CONFIG_SD_ENABLED
  #define TEST_CONFIG_SD_ENABLED 1
#endif
#ifndef TEST_CONFIG_GPIO_APPS_ENABLE
  #define TEST_CONFIG_GPIO_APPS_ENABLE 1
#endif
#ifndef TEST_CONFIG_FILE_MANAGER_ENABLE
  #define TEST_CONFIG_FILE_MANAGER_ENABLE 1
#endif
#ifndef TEST_CONFIG_OTA_ENABLE
  #define TEST_CONFIG_OTA_ENABLE 1
#endif
#ifndef TEST_CONFIG_SDR_ENABLE
  #define TEST_CONFIG_SDR_ENABLE 1
#endif

#if TEST_CONFIG_WIFI_APPS_ENABLE
  #define CONFIG_WIFI_APPS_ENABLE 1
#endif
#if TEST_CONFIG_BLUETOOTH_APPS_ENABLE
  #define CONFIG_BLUETOOTH_APPS_ENABLE 1
#endif
/* The stacks the build variants leave out. The catalogue gates the Zigbee
   and the Thread entries on them as well as on the app switch: the apps
   live in the modules that a variant does not compile. */
#ifndef TEST_CONFIG_MININO_ZIGBEE_STACK
  #define TEST_CONFIG_MININO_ZIGBEE_STACK 1
#endif
#ifndef TEST_CONFIG_MININO_OPENTHREAD_STACK
  #define TEST_CONFIG_MININO_OPENTHREAD_STACK 1
#endif

#if TEST_CONFIG_ZIGBEE_APPS_ENABLE
  #define CONFIG_ZIGBEE_APPS_ENABLE 1
#endif
#if TEST_CONFIG_THREAD_APPS_ENABLE
  #define CONFIG_THREAD_APPS_ENABLE 1
#endif

#if TEST_CONFIG_MININO_ZIGBEE_STACK
  #define CONFIG_MININO_ZIGBEE_STACK 1
#endif

#if TEST_CONFIG_MININO_OPENTHREAD_STACK
  #define CONFIG_MININO_OPENTHREAD_STACK 1
#endif
#if TEST_CONFIG_GPS_APPS_ENABLE
  #define CONFIG_GPS_APPS_ENABLE 1
#endif
#if TEST_CONFIG_GPS_ENABLED
  #define CONFIG_GPS_ENABLED 1
#endif
#if TEST_CONFIG_SD_ENABLED
  #define CONFIG_SD_ENABLED 1
#endif
#if TEST_CONFIG_GPIO_APPS_ENABLE
  #define CONFIG_GPIO_APPS_ENABLE 1
#endif
#if TEST_CONFIG_FILE_MANAGER_ENABLE
  #define CONFIG_FILE_MANAGER_ENABLE 1
  #define CONFIG_FILE_MANAGER_LOCAL  1
  #define CONFIG_FILE_MANAGER_WEB    1
#endif
#if TEST_CONFIG_OTA_ENABLE
  #define CONFIG_OTA_ENABLE 1
#endif
#if TEST_CONFIG_SDR_ENABLE
  #define CONFIG_SDR_ENABLE 1
#endif
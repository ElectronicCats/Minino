#include "screen_saver.h"

#include "bitmaps_general.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"

static int IDLE_TIMEOUT_S = 30;

static volatile bool screen_saver_running;
esp_timer_handle_t screen_savar_idle_timer2;

void screen_saver_run();

static void timer_callback() {
  if (menus_module_get_app_state() || screen_saver_running) {
    return;
  }

  menu_idx_t menu = menus_module_get_current_menu();
  if (menu == MENU_WIFI_ANALYZER_RUN_2 ||
      menu == MENU_WIFI_ANALYZER_SUMMARY_2 || menu == MENU_GPS_DATE_TIME_2 ||
      menu == MENU_GPS_LOCATION_2 || menu == MENU_GPS_SPEED_2) {
    return;
  }

  screen_saver_run();
}

static void show_splash_screen() {
  int get_logo = preferences_get_int("dp_select", 0);
  epd_bitmap_t logo;
  logo = screen_savers[get_logo];

  screen_saver_running = true;
  int w_screen_space = SCREEN_WIDTH2 - logo.width;
  int h_screen_space = SCREEN_HEIGHT2 - logo.height;
  int start_x_position = w_screen_space / 2;
  static int start_y_position = 16;
  static int x_direction = 1;
  static int y_direction = 1;

  while (screen_saver_running) {
    // oled_screen_clear_buffer();
    oled_screen_display_bitmap(logo.bitmap, start_x_position, start_y_position,
                               logo.width, logo.height, OLED_DISPLAY_NORMAL);

    start_x_position += x_direction;
    start_y_position += y_direction;

    if (start_x_position <= 0 || start_x_position >= w_screen_space - 2) {
      x_direction = -x_direction;
    }
    if (start_y_position <= 0 || start_y_position >= h_screen_space) {
      y_direction = -y_direction;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  vTaskDelete(NULL);
}

void screen_saver_run() {
  oled_screen_clear();
  xTaskCreate(show_splash_screen, "show_splash_screen", 4096, NULL, 5, NULL);
}

void screen_saver_stop() {
  screen_saver_running = false;
}

void screen_saver_set_idle_timeout(uint8_t timeout_seconds) {
  IDLE_TIMEOUT_S = timeout_seconds;
}

bool screen_saver_get_idle_state() {
  bool idle = screen_saver_running;
  screen_saver_stop();
  esp_timer_stop(screen_savar_idle_timer2);
  esp_timer_start_once(screen_savar_idle_timer2, IDLE_TIMEOUT_S * 1000 * 1000);
  return idle;
}

void screen_saver_begin() {
  esp_timer_create_args_t timer_args = {
      .callback = timer_callback, .arg = NULL, .name = "idle_timer"};
  esp_err_t err = esp_timer_create(&timer_args, &screen_savar_idle_timer2);
}
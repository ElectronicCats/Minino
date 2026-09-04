#include "screen_saver.h"

#include "bitmaps_general.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"

static int IDLE_TIMEOUT_S = 30;

static volatile bool screen_saver_running = false;
static TaskHandle_t screen_saver_task_handle = NULL;
static esp_timer_handle_t screen_saver_idle_timer;

void screen_saver_run();

static void timer_callback() {
  if (menus_module_get_app_state() || screen_saver_running) {
    return;
  }

  menu_idx_t menu = menus_module_get_current_menu();
  if (menu == MENU_WIFI_ANALYZER_RUN || menu == MENU_WIFI_ANALYZER_SUMMARY ||
      menu == MENU_GPS_DATE_TIME || menu == MENU_GPS_LOCATION ||
      menu == MENU_GPS_SPEED) {
    return;
  }

  screen_saver_run();
}

static void show_splash_screen(void* pvParameters) {
  uint8_t screen_savers_count = sizeof(screen_savers) / sizeof(epd_bitmap_t*);
  if (screen_savers_count == 0) {
    screen_saver_running = false;
    screen_saver_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }
  int get_logo =
      MIN(screen_savers_count - 1, preferences_get_int("dp_select", 0));
  if (get_logo < 0) {
    get_logo = 0;
  }
  const epd_bitmap_t* logo = screen_savers[get_logo];

  screen_saver_running = true;
  int w_screen_space = SCREEN_WIDTH2 - logo->width;
  int h_screen_space = SCREEN_HEIGHT2 - logo->height;
  if (w_screen_space < 0) {
    w_screen_space = 0;
  }
  if (h_screen_space < 0) {
    h_screen_space = 0;
  }

  int start_x_position = w_screen_space / 2;
  int start_y_position = h_screen_space / 2;
  int x_direction = (w_screen_space > 0) ? 1 : 0;
  int y_direction = (h_screen_space > 0) ? 1 : 0;

  while (screen_saver_running) {
    oled_screen_display_bitmap(logo->bitmap, start_x_position, start_y_position,
                               logo->width, logo->height, OLED_DISPLAY_NORMAL);
    oled_screen_display_show();

    start_x_position += x_direction;
    start_y_position += y_direction;

    if (w_screen_space > 0) {
      if (start_x_position <= 0) {
        start_x_position = 0;
        x_direction = 1;
      } else if (start_x_position >= w_screen_space) {
        start_x_position = w_screen_space;
        x_direction = -1;
      }
    }

    if (h_screen_space > 0) {
      if (start_y_position <= 0) {
        start_y_position = 0;
        y_direction = 1;
      } else if (start_y_position >= h_screen_space) {
        start_y_position = h_screen_space;
        y_direction = -1;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  screen_saver_task_handle = NULL;
  vTaskDelete(NULL);
}

void screen_saver_run() {
  if (screen_saver_running || screen_saver_task_handle != NULL) {
    return;
  }
  oled_screen_clear();
  xTaskCreate(show_splash_screen, "show_splash_screen", 2048, NULL, 5,
              &screen_saver_task_handle);
}

void screen_saver_stop() {
  screen_saver_running = false;
}

void screen_saver_set_idle_timeout(uint8_t timeout_seconds) {
  IDLE_TIMEOUT_S = timeout_seconds;
  preferences_put_int("dp_time", IDLE_TIMEOUT_S);
}

bool screen_saver_get_idle_state() {
  bool idle = screen_saver_running;
  screen_saver_stop();
  esp_timer_stop(screen_saver_idle_timer);
  esp_timer_start_once(screen_saver_idle_timer, (uint64_t)IDLE_TIMEOUT_S * 1000 * 1000);
  return idle;
}

void screen_saver_begin() {
  IDLE_TIMEOUT_S = preferences_get_int("dp_time", 30);
  esp_timer_create_args_t timer_args = {
      .callback = timer_callback, .arg = NULL, .name = "idle_timer"};
  esp_timer_create(&timer_args, &screen_saver_idle_timer);
}
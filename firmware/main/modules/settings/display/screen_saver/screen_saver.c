#include "screen_saver.h"

#include "bitmaps_general.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"

static const char* TAG = "screen_saver";

static int IDLE_TIMEOUT_S = 30;

static volatile bool screen_saver_running = false;
static TaskHandle_t screen_saver_task_handle = NULL;
static esp_timer_handle_t screen_saver_idle_timer;
static SemaphoreHandle_t screen_saver_exit_sem = NULL;

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
  uint8_t screen_savers_count = SCREEN_SAVERS_COUNT;
  if (screen_savers_count == 0) {
    screen_saver_running = false;
    screen_saver_task_handle = NULL;
    if (screen_saver_exit_sem != NULL) {
      xSemaphoreGive(screen_saver_exit_sem);
    }
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
  oled_screen_clear();
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
  if (screen_saver_exit_sem != NULL) {
    xSemaphoreGive(screen_saver_exit_sem);
  }
  vTaskDelete(NULL);
}

void screen_saver_run() {
  if (screen_saver_running || screen_saver_task_handle != NULL) {
    return;
  }
  /* Clear only once the task is known to exist. Clearing first and failing to
     create it would leave the panel blank with nothing animating - the caller
     (including show_logo() at boot) would just wait for a screensaver that
     never appears. */
  if (screen_saver_exit_sem == NULL) {
    screen_saver_exit_sem = xSemaphoreCreateBinary();
    if (screen_saver_exit_sem == NULL) {
      ESP_LOGE(TAG, "failed to create exit semaphore");
      return;
    }
  } else {
    /* Drain any stale token */
    xSemaphoreTake(screen_saver_exit_sem, 0);
  }

  screen_saver_running = true;
  TaskHandle_t handle = NULL;
  /* Use priority 4 (same as input_task) to avoid priority inversion where
     the higher-priority screensaver preempts the menu render after wake-up. */
  BaseType_t ok = xTaskCreate(show_splash_screen, "show_splash_screen", 2048,
                              NULL, 4, &handle);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "could not create the screensaver task");
    screen_saver_running = false;
    screen_saver_task_handle = NULL;
    return;
  }
  /* show_splash_screen() draws the first frame itself; it draws over whatever
     is on the panel, so there is no need to blank it here. */
  screen_saver_task_handle = handle;
}

void screen_saver_stop() {
  /* Clear the flag first so the task leaves its loop, then wait for it to be
     gone via the exit semaphore. Clearing the flag alone is not enough:
     show_splash_screen() finishes the iteration it is in - display_bitmap() +
     display_show() - before it re-checks the flag. A caller that draws
     immediately after this would have its frame overwritten by the animation's
     last one, leaving a black rectangle on the panel. That is exactly what
     happened once the button callbacks moved off the esp_timer task (priority
     22) into a dedicated one (priority 4): the animation at priority 5
     preempted the menu render.
     Waiting on the semaphore makes the contract "stopped when this returns"
     true whatever the caller's priority is. Bounded, because
     screen_saver_stop() must never hang the UI. */
  screen_saver_running = false;

  if (screen_saver_task_handle != NULL) {
    /* Wait for the task to signal exit via semaphore (max 1 s) */
    if (screen_saver_exit_sem != NULL) {
      if (xSemaphoreTake(screen_saver_exit_sem, pdMS_TO_TICKS(1000)) !=
          pdTRUE) {
        ESP_LOGW(TAG, "screensaver task did not exit in time, forcing delete");
        vTaskDelete(screen_saver_task_handle);
      }
    } else {
      /* Fallback: poll task handle */
      uint8_t retries = 100;
      while (screen_saver_task_handle != NULL && retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      if (screen_saver_task_handle != NULL) {
        ESP_LOGW(TAG, "screensaver task still alive after 1 s, forcing delete");
        vTaskDelete(screen_saver_task_handle);
      }
    }
    screen_saver_task_handle = NULL;
  }
}

void screen_saver_set_idle_timeout(uint8_t timeout_seconds) {
  IDLE_TIMEOUT_S = timeout_seconds;
  preferences_put_int("dp_time", IDLE_TIMEOUT_S);
}

bool screen_saver_get_idle_state() {
  bool idle = screen_saver_running || (screen_saver_task_handle != NULL);
  screen_saver_stop();
  esp_timer_stop(screen_saver_idle_timer);
  esp_timer_start_once(screen_saver_idle_timer,
                       (uint64_t) IDLE_TIMEOUT_S * 1000 * 1000);
  return idle;
}

void screen_saver_begin() {
  IDLE_TIMEOUT_S = preferences_get_int("dp_time", 30);
  esp_timer_create_args_t timer_args = {
      .callback = timer_callback, .arg = NULL, .name = "idle_timer"};
  esp_timer_create(&timer_args, &screen_saver_idle_timer);
}
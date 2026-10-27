#include "keyboard_module.h"
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "preferences.h"

static int IDLE_TIMEOUT_S = 30;

static const char* TAG = "keyboard";
static input_callback_t input_callback = NULL;
static esp_timer_handle_t idle_timer = NULL;
static bool lock_input = false;

/*
 * Button events are queued to a task of our own instead of being dispatched
 * inline.
 *
 * iot_button polls its GPIO pins from an esp_timer created with
 * dispatch_method = ESP_TIMER_TASK, i.e. the callback ran on the esp_timer
 * task: priority 22, stack 3584 bytes, and the task every esp_timer in the
 * system depends on. From there button_event_cb() called into menus_input_cb()
 * and from there into every app's on_enter_cb / select_cb. Several of those
 * block for a long time - sd_card_mount(), nvs_flash_init(),
 * esp_openthread_init(), vTaskDelay() of a second or more, or
 * esp_openthread_lock_acquire(portMAX_DELAY) - and while they did, the button
 * debounce timer, the LED effects, the screen-saver/AFK timers, the heap
 * monitor and the Wi-Fi timers all stopped. Mounting the SD card from a key
 * press froze the UI for as long as the mount took.
 *
 * The queue keeps the ordering (single consumer, FIFO) while moving the work
 * off the timer task, so the rest of the timers keep ticking.
 */
#define INPUT_QUEUE_LEN     16
#define INPUT_TASK_STACK    4096
#define INPUT_TASK_PRIORITY 4

typedef struct {
  uint8_t button_name;
  uint8_t button_event;
} input_event_t;

static QueueHandle_t s_input_queue = NULL;
static TaskHandle_t s_input_task = NULL;
static SemaphoreHandle_t s_input_state_mux = NULL;
/* Dropped events are a symptom worth surfacing, not something to retry: a
 * queued backlog means an app callback is blocking for a long time. */
static volatile uint32_t s_dropped_events = 0;

static const char* button_to_name[] = {
    "BOOT", "LEFT", "RIGHT", "UP", "DOWN",
};

static const char* event_to_name[] = {
    "PRESS_DOWN",      "PRESS_UP",      "PRESS_REPEAT",   "PRESS_REPEAT_DONE",
    "SINGLE_CLICK",    "DOUBLE_CLICK",  "MULTIPLE_CLICK", "LONG_PRESS_START",
    "LONG_PRESS_HOLD", "LONG_PRESS_UP", "PRESS_END",      "NONE_PRESS",
};

static void button_event_cb(void* arg, void* data);

static void keyboard_idle_timer_cb(void* arg) {
  // Idle callback placeholder
}

static void input_task(void* arg) {
  (void) arg;
  input_event_t ev;
  while (true) {
    if (xQueueReceive(s_input_queue, &ev, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    /* Logged here rather than in the esp_timer task: this is a UART write, and
       the USB console is the same sink the REPL uses. */
    ESP_LOGI(TAG, "Button %s event %s", button_to_name[ev.button_name],
             event_to_name[ev.button_event]);

    input_callback_t cb = NULL;
    bool locked = false;
    if (s_input_state_mux != NULL) {
      xSemaphoreTake(s_input_state_mux, portMAX_DELAY);
      cb = input_callback;
      locked = lock_input;
      xSemaphoreGive(s_input_state_mux);
    } else {
      cb = input_callback;
      locked = lock_input;
    }

    if (locked || cb == NULL) {
      continue;
    }
    cb(ev.button_name, ev.button_event);
  }
}

static bool input_task_start(void) {
  if (s_input_queue == NULL) {
    s_input_queue = xQueueCreate(INPUT_QUEUE_LEN, sizeof(input_event_t));
    if (s_input_queue == NULL) {
      ESP_LOGE(TAG, "could not create the input queue");
      return false;
    }
  }
  if (s_input_state_mux == NULL) {
    s_input_state_mux = xSemaphoreCreateMutex();
    if (s_input_state_mux == NULL) {
      ESP_LOGE(TAG, "could not create the input state mutex");
      return false;
    }
  }
  if (s_input_task != NULL) {
    return true;
  }
  if (xTaskCreate(input_task, "input_task", INPUT_TASK_STACK, NULL,
                  INPUT_TASK_PRIORITY, &s_input_task) != pdPASS) {
    s_input_task = NULL;
    ESP_LOGE(TAG, "could not create the input task");
    return false;
  }
  return true;
}

void keyboard_module_reset_idle_timer() {
  if (idle_timer != NULL) {
    esp_timer_stop(idle_timer);
    esp_timer_start_once(idle_timer, (uint64_t) IDLE_TIMEOUT_S * 1000 * 1000);
  }
}

void keyboard_module_set_lock(bool lock) {
  if (s_input_state_mux != NULL) {
    xSemaphoreTake(s_input_state_mux, portMAX_DELAY);
    lock_input = lock;
    xSemaphoreGive(s_input_state_mux);
  } else {
    lock_input = lock;
  }
}

void button_init(uint32_t button_num, uint8_t mask) {
  button_config_t btn_cfg = {
      .type = BUTTON_TYPE_GPIO,
      .gpio_button_config =
          {
              .gpio_num = button_num,
              .active_level = BUTTON_ACTIVE_LEVEL,
          },
  };
  button_handle_t btn = iot_button_create(&btn_cfg);
  if (!btn) {
    ESP_LOGE("keyboard_module", "Failed to create button %" PRIu32, button_num);
    return;
  }
  iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_PRESS_DOWN | mask));
  iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_PRESS_UP | mask));
  iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_PRESS_REPEAT | mask));
  iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_PRESS_REPEAT_DONE | mask));
  iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_SINGLE_CLICK | mask));
  iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_DOUBLE_CLICK | mask));
  iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_LONG_PRESS_START | mask));
  iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_LONG_PRESS_HOLD | mask));
  iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_LONG_PRESS_UP | mask));
}

/**
 * @brief iot_button event callback - runs on the esp_timer task.
 *
 * Decodes the packed argument and hands it to the input task. Nothing else:
 * no logging, no callbacks, nothing that can block.
 *
 * @param void* arg
 * @param void* data packed (button << 4) | event
 */
static void button_event_cb(void* arg, void* data) {
  (void) arg;
  uint8_t button_name = ((((button_event_t) data) >> 4));
  uint8_t button_event = (((button_event_t) data) & 0x0F);

  if (button_name >= (sizeof(button_to_name) / sizeof(button_to_name[0])) ||
      button_event >= (sizeof(event_to_name) / sizeof(event_to_name[0]))) {
    /* Indices feed the name tables above; a bad value would read past them. */
    return;
  }

  if (s_input_queue == NULL) {
    return;
  }

  input_event_t ev = {.button_name = button_name, .button_event = button_event};
  /* Zero timeout: never block the esp_timer task. A full queue means an app
     callback is still running; dropping is better than stalling every timer in
     the system. */
  if (xQueueSend(s_input_queue, &ev, 0) != pdTRUE) {
    s_dropped_events++;
    if ((s_dropped_events % 16) == 1) {
      ESP_LOGW(TAG, "input queue full, dropped %u event(s) so far",
               (unsigned) s_dropped_events);
    }
  }
}

void keyboard_module_set_input_callback(input_callback_t input_cb) {
  if (s_input_state_mux != NULL) {
    xSemaphoreTake(s_input_state_mux, portMAX_DELAY);
    input_callback = input_cb;
    xSemaphoreGive(s_input_state_mux);
  } else {
    input_callback = input_cb;
  }
}

uint32_t keyboard_module_dropped_events(void) {
  return s_dropped_events;
}

bool keyboard_module_inject_event(uint8_t button, uint8_t event) {
  if (button >= (sizeof(button_to_name) / sizeof(button_to_name[0])) ||
      event >= (sizeof(event_to_name) / sizeof(event_to_name[0]))) {
    return false;
  }
  if (s_input_queue == NULL) {
    return false;
  }
  input_event_t ev = {.button_name = button, .button_event = event};
  return xQueueSend(s_input_queue, &ev, 0) == pdTRUE;
}

void keyboard_module_begin() {
  esp_log_level_set(TAG, ESP_LOG_INFO);
  if (!input_task_start()) {
    ESP_LOGE(TAG, "no input task; button events will be ignored");
    return;
  }
  if (idle_timer == NULL) {
    const esp_timer_create_args_t timer_args = {
        .callback = keyboard_idle_timer_cb,
        .arg = NULL,
        .name = "kbd_idle_timer",
    };
    esp_timer_create(&timer_args, &idle_timer);
  }
  button_init(BOOT_BUTTON_PIN, BOOT_BUTTON_MASK);
  button_init(LEFT_BUTTON_PIN, LEFT_BUTTON_MASK);
  button_init(RIGHT_BUTTON_PIN, RIGHT_BUTTON_MASK);
  button_init(UP_BUTTON_PIN, UP_BUTTON_MASK);
  button_init(DOWN_BUTTON_PIN, DOWN_BUTTON_MASK);
}
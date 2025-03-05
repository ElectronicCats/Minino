#include "animations_module.h"

#include "esp_log.h"
#include "freertos/semphr.h"

#include "oled_screen.h"

#define TAG "ANIMATIONS_MODULE"

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

static SemaphoreHandle_t anim_mutex = NULL;
static animations_module_ctx_t* anim_ctx = NULL;

///////////////////////////////////////////////////////////

static void anim_mutex_alloc() {
  if (anim_mutex) {
    return;
  }
  anim_mutex = xSemaphoreCreateMutex();
}

///////////////////////////////////////////////////////////

static void anim_ctx_free() {
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  if (anim_ctx) {
    free(anim_ctx);
    anim_ctx = NULL;
  }
  xSemaphoreGive(anim_mutex);
}

static void set_running(bool val) {
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  anim_ctx->_is_runing = val;
  xSemaphoreGive(anim_mutex);
}
static bool get_running() {
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  bool _is_running = anim_ctx->_is_runing;
  xSemaphoreGive(anim_mutex);
  return _is_running;
}

static void set_paused(bool val) {
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  anim_ctx->_is_paused = val;
  xSemaphoreGive(anim_mutex);
}
static bool get_paused() {
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  bool _is_paused = anim_ctx->_is_paused;
  xSemaphoreGive(anim_mutex);
  return _is_paused;
}

void animations_module_set_pos(uint8_t x, uint8_t y) {
  // if (x >= OLED_WIDTH || y >= OLED_HEIGHT)
  // {
  //     return;
  // }
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  if (!anim_ctx) {
    xSemaphoreGive(anim_mutex);
    return;
  }
  anim_ctx->x = x;
  anim_ctx->y = y;
  xSemaphoreGive(anim_mutex);
}

static void task_delay() {
  set_paused(true);
  vTaskDelay(anim_ctx->animation->duration_ms[anim_ctx->current_frame]);
  set_paused(false);
}

static void increment_frame() {
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  if (++anim_ctx->current_frame >= anim_ctx->animation->frames_len) {
    if (anim_ctx->loop) {
      anim_ctx->current_frame = 0;
    } else {
      anim_ctx->_is_runing = false;
    }
  }
  xSemaphoreGive(anim_mutex);
}

static void draw_frame() {
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  if (!anim_ctx->manual_clear) {
    oled_screen_clear_buffer();
  }

  if (anim_ctx->pre_draw_cb) {
    anim_ctx->pre_draw_cb();
  }

  uint8_t actual_frame = anim_ctx->animation->order[anim_ctx->current_frame];
  if (actual_frame >= anim_ctx->animation->bitmaps_len) {
    anim_ctx->_is_runing = false;
    xSemaphoreGive(anim_mutex);
    return;
  }
  bitmap_t* bitmap = &anim_ctx->animation->bitmaps[actual_frame];

  oled_screen_buffer_bitmap(bitmap->bitmap, anim_ctx->x, anim_ctx->y,
                            bitmap->width, bitmap->height, anim_ctx->invert);

  if (anim_ctx->pos_draw_cb) {
    anim_ctx->pos_draw_cb();
  }

  if (!anim_ctx->manual_show) {
    oled_screen_display_show();
  }
  xSemaphoreGive(anim_mutex);
}

static void animation_task() {
  set_running(true);
  while (get_running()) {
    draw_frame();
    increment_frame();
    if (!get_running()) {
      break;
    }
    task_delay();
  }
  void (*exit_cb)() = anim_ctx->exit_cb;
  anim_ctx_free();
  if (exit_cb) {
    exit_cb();
  }
  vTaskDelete(NULL);
}

void animations_module_pause() {
  if (!anim_mutex) {
    ESP_LOGW(TAG, "Run any animation first");
    return;
  }
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  if (anim_ctx && anim_ctx->task_handle) {
    xSemaphoreGive(anim_mutex);
    while (!get_paused()) {
      vTaskDelay(1);
    }
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    vTaskSuspend(anim_ctx->task_handle);
  } else {
    ESP_LOGW(TAG, "There is not any running Task");
  }
  xSemaphoreGive(anim_mutex);
}

void animations_module_delete() {
  animations_module_pause();
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  if (anim_ctx && anim_ctx->task_handle) {
    vTaskDelete(anim_ctx->task_handle);
    xSemaphoreGive(anim_mutex);
    anim_ctx_free();
  }
  xSemaphoreGive(anim_mutex);
}

void animations_module_resume() {
  if (!anim_mutex) {
    ESP_LOGW(TAG, "Run any animation first");
    return;
  }
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  if (anim_ctx && anim_ctx->task_handle && anim_ctx->_is_paused &&
      anim_ctx->_is_runing) {
    vTaskResume(anim_ctx->task_handle);
  } else {
    ESP_LOGW(TAG, "There is not any paused Task");
  }
  xSemaphoreGive(anim_mutex);
}

void animations_module_stop() {
  if (!anim_mutex) {
    ESP_LOGW(TAG, "Run any animation first");
    return;
  }
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  if (anim_ctx && anim_ctx->task_handle && anim_ctx->_is_runing) {
    anim_ctx->_is_runing = false;
  }
  xSemaphoreGive(anim_mutex);
}

void animations_module_run(animations_module_ctx_t ctx) {
  anim_mutex_alloc();

  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  if (anim_ctx) {
    xSemaphoreGive(anim_mutex);
    ESP_LOGW(TAG,
             "Another animation is running, stop it first by calling "
             "animations_module_stop()");
    return;
  }
  xSemaphoreGive(anim_mutex);

  anim_ctx_free();
  xSemaphoreTake(anim_mutex, portMAX_DELAY);
  anim_ctx = calloc(1, sizeof(animations_module_ctx_t));

  anim_ctx->animation = ctx.animation;
  anim_ctx->current_frame = ctx.current_frame;
  anim_ctx->pre_draw_cb = ctx.pre_draw_cb;
  anim_ctx->pos_draw_cb = ctx.pos_draw_cb;
  anim_ctx->exit_cb = ctx.exit_cb;
  anim_ctx->invert = ctx.invert;
  anim_ctx->loop = ctx.loop;
  anim_ctx->x = ctx.x;
  anim_ctx->y = ctx.y;
  anim_ctx->manual_clear = ctx.manual_clear;
  anim_ctx->manual_show = ctx.manual_show;

  xTaskCreate(animation_task, "animation_task", 4096, NULL, 10,
              &anim_ctx->task_handle);

  xSemaphoreGive(anim_mutex);
}
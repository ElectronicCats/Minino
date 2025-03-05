#ifndef _ANIMATIONS_MODULE_H_
#define _ANIMATIONS_MODULE_H_

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "animation_t.h"

typedef struct {
  const animation_t* animation;
  uint8_t x;
  uint8_t y;
  uint16_t current_frame;
  TaskHandle_t task_handle;
  void (*pre_draw_cb)();
  void (*pos_draw_cb)();
  void (*exit_cb)();
  bool invert;
  bool loop;
  bool manual_clear;
  bool manual_show;
  volatile bool _is_runing;
  volatile bool _is_paused;
} animations_module_ctx_t;

void animations_module_run(animations_module_ctx_t ctx);
void animations_module_stop();
void animations_module_resume();
void animations_module_pause();
void animations_module_delete();
void animations_module_set_pos(uint8_t x, uint8_t y);

#endif  // ANIMATIONS_MODULE_H_
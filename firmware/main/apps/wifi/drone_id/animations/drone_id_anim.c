#include "drone_id_anim.h"

#include <string.h>

#include "animation_t.h"
#include "animations_module.h"
#include "oled_screen.h"

#include "drone_id_bmps.h"

////////////////////// DRONE ANIM /////////////////////
const uint16_t drone_anim_order[] = {0, 1, 2, 1};
const uint32_t drone_anim_durations_ms[] = {50, 50, 50, 50};

const animation_t drone_animation = {
    .bitmaps = drone_id_bitmaps,
    .bitmaps_len = sizeof(drone_id_bitmaps) / sizeof(bitmap_t),
    .order = drone_anim_order,
    .duration_ms = drone_anim_durations_ms,
    .frames_len = sizeof(drone_anim_order) / sizeof(uint16_t)};

static void pos_scan_draw() {}

void drone_anim_scan() {
  animations_module_ctx_t animation = {0};
  animation.animation = &drone_animation;
  animation.loop = true;
  animation.x = 110;
  animation.pos_draw_cb = pos_scan_draw;
  animation.manual_clear = true;

  animations_module_run(animation);
}

void drone_anim_stop_any() {
  animations_module_delete();
}
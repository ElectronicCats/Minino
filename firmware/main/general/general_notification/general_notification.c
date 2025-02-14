#include "general_notification.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "menus_module.h"
#include "oled_screen.h"

general_notification_ctx_t* notification_ctx = NULL;
static void free_ctx();

static void notification_handler(uint8_t button, uint8_t event) {
  if (button != BUTTON_LEFT || event != BUTTON_PRESS_DOWN) {
    return;
  }
  void (*exit_cb)() = notification_ctx->on_exit;
  free_ctx();
  if (exit_cb) {
    exit_cb();
  }
}

static void draw_notification() {
  oled_screen_clear();
  oled_screen_display_card_border();
  int page = 2;
  oled_screen_display_text_center(notification_ctx->head, page,
                                  OLED_DISPLAY_NORMAL);
  page++;
  if (strlen(notification_ctx->body) > MAX_LINE_CHAR) {
    oled_screen_display_text_splited(notification_ctx->body, &page,
                                     OLED_DISPLAY_NORMAL);
    oled_screen_display_show();
    return;
  }
  oled_screen_display_text_center(notification_ctx->body, page,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
}

static void free_ctx() {
  free(notification_ctx);
  notification_ctx = NULL;
}

void general_notification(general_notification_ctx_t ctx) {
  if (notification_ctx) {
    free_ctx();
  }
  notification_ctx = calloc(1, sizeof(general_notification_ctx_t));
  notification_ctx->head = ctx.head;
  notification_ctx->body = ctx.body;
  notification_ctx->on_enter = ctx.on_enter;
  notification_ctx->on_exit = ctx.on_exit;
  notification_ctx->duration_ms = ctx.duration_ms;

  menus_module_disable_input();
  if (notification_ctx->on_enter) {
    notification_ctx->on_enter();
  }
  draw_notification();
  vTaskDelay(pdMS_TO_TICKS(notification_ctx->duration_ms));
  menus_module_enable_input();
  void (*exit_cb)() = notification_ctx->on_exit;
  free_ctx();
  if (exit_cb) {
    exit_cb();
  }
}

void general_notification_handler(general_notification_ctx_t ctx) {
  if (notification_ctx) {
    free_ctx();
  }
  notification_ctx = calloc(1, sizeof(general_notification_ctx_t));
  notification_ctx->head = ctx.head;
  notification_ctx->body = ctx.body;
  notification_ctx->on_enter = ctx.on_enter;
  notification_ctx->on_exit = ctx.on_exit;
  notification_ctx->duration_ms = ctx.duration_ms;

  if (notification_ctx->on_enter) {
    notification_ctx->on_enter();
  }
  draw_notification();
  menus_module_set_app_state(true, notification_handler);
}
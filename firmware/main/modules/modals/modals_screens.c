#include "modals_screens.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oled_screen.h"

#define MAX_OPTIONS_NUM 7

void modals_screens_list_y_n_options_cb(modal_get_user_selection_t* ctx) {
  oled_screen_clear();
  oled_screen_display_text(ctx->banner, 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(ctx->options[0], 3,
                                  ctx->selected_option == 0);
  oled_screen_display_text_center(ctx->options[1], 4,
                                  ctx->selected_option == 1);
}

void modals_screens_default_list_options_cb(modal_get_user_selection_t* ctx) {
  static uint8_t items_offset = 0;
  items_offset = MAX(ctx->selected_option - 6, items_offset);
  items_offset = MIN(ctx->selected_option, items_offset);
  oled_screen_clear();
  oled_screen_display_text(ctx->banner, 0, 0, OLED_DISPLAY_NORMAL);
  for (uint8_t i = 0; i < (MIN(ctx->options_count, MAX_OPTIONS_NUM - 1)); i++) {
    oled_screen_display_text_center(ctx->options[i + items_offset], i + 2,
                                    ctx->selected_option == i + items_offset);
  }
}

void modals_screens_show_info(char* head, char* body, size_t time_ms) {
  oled_screen_clear();
  int page = 2;
  oled_screen_display_text_center(head, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_splited(body, &page, OLED_DISPLAY_NORMAL);
  vTaskDelay(pdMS_TO_TICKS(time_ms));
  oled_screen_clear();
}
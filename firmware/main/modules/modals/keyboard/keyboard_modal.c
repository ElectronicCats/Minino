#include "keyboard_modal.h"

#include <string.h>

#include "keyboard_module.h"
#include "keyboard_screens.h"
#include "menus_module.h"

#define TAG "Keyboard Modal"

static keyboard_modal_ctx_t* kb_ctx;

static void keyboard_event_press_down(uint8_t button_name) {
  switch (button_name) {
    case BUTTON_LEFT:
      kb_ctx->current_char = kb_ctx->current_char == 0
                                 ? kb_ctx->text_length - 1
                                 : kb_ctx->current_char - 1;
      keyboard_screens_update_text(kb_ctx);
      break;
    case BUTTON_RIGHT:
      kb_ctx->current_char = (kb_ctx->current_char + 1 < kb_ctx->text_length)
                                 ? (kb_ctx->current_char + 1)
                                 : 0;
      keyboard_screens_update_text(kb_ctx);
      break;
    case BUTTON_UP: {
      char ch = kb_ctx->new_text[kb_ctx->current_char];
      if (ch >= '0' && ch < '9') {
        ch++;
      } else if (ch == '9') {
        ch = 'a';
      } else if (ch >= 'a' && ch < 'z') {
        ch++;
      } else {
        ch = '0';
      }
      kb_ctx->new_text[kb_ctx->current_char] = ch;
      keyboard_screens_update_text(kb_ctx);
      break;
    }
    case BUTTON_DOWN: {
      char ch = kb_ctx->new_text[kb_ctx->current_char];
      if (ch > 'a' && ch <= 'z') {
        ch--;
      } else if (ch == 'a') {
        ch = '9';
      } else if (ch > '0' && ch <= '9') {
        ch--;
      } else {
        ch = 'z';
      }
      kb_ctx->new_text[kb_ctx->current_char] = ch;
      keyboard_screens_update_text(kb_ctx);
      break;
    }
    case BUTTON_BOOT:
      break;
    default:
      break;
  }
}

static void keyboard_modal_input_cb(uint8_t button_name, uint8_t button_event) {
  switch (button_event) {
    case BUTTON_PRESS_DOWN:
      keyboard_event_press_down(button_name);
      break;
    case BUTTON_LONG_PRESS_START:
      if (button_name == BUTTON_RIGHT) {
        kb_ctx->consumed = 1;
      } else if (button_name == BUTTON_LEFT) {
        kb_ctx->consumed = -1;
      }
      break;
    default:
      break;
  }
}

static void keyboard_modal_alloc(char* text, char* banner) {
  kb_ctx = malloc(sizeof(keyboard_modal_ctx_t));
  memset(kb_ctx, 0, sizeof(keyboard_modal_ctx_t));
  kb_ctx->old_text = text;
  kb_ctx->banner = banner;
  kb_ctx->text_length = strlen(text);
  kb_ctx->new_text = (char*) malloc(kb_ctx->text_length + 1);
  strcpy(kb_ctx->new_text, text);
}

char* keyboard_modal_write(char* text, char* banner) {
  keyboard_modal_alloc(text, banner);
  menus_module_set_app_state(true, keyboard_modal_input_cb);
  keyboard_screens_show(kb_ctx);
  keyboard_screens_update_text(kb_ctx);
  while (!kb_ctx->consumed) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  char* new_text = NULL;
  if (kb_ctx->consumed > 0) {
    new_text = strdup(kb_ctx->new_text);
  }
  if (kb_ctx->new_text != NULL) {
    free(kb_ctx->new_text);
    kb_ctx->new_text = NULL;
  }
  free(kb_ctx);
  kb_ctx = NULL;
  return new_text;
}
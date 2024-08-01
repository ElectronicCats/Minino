#include "oled_screen.h"

void sd_card_settings_screens_module_no_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("No SD Card", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("detected!", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", 5, OLED_DISPLAY_INVERT);
}

void sd_card_settings_screens_module_sd_card_ok() {
  oled_screen_clear();
  oled_screen_display_text_center("SD Card can", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("be used safety", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", 5, OLED_DISPLAY_INVERT);
}

void sd_card_settings_screens_module_format_question() {
  oled_screen_clear();
  oled_screen_display_text_center("SD Card is not", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("in FAT32,", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Format?", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", 5, OLED_DISPLAY_INVERT);
}

void sd_card_settings_screens_module_formating_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("Formating SD", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Card... don't", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("remove it!", 4, OLED_DISPLAY_NORMAL);
}

void sd_card_settings_screens_module_failed_format_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("Failed to", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("format SD Card", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", 5, OLED_DISPLAY_INVERT);
}

void sd_card_settings_screens_module_format_done() {
  oled_screen_clear();
  oled_screen_display_text_center("Format done!", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", 5, OLED_DISPLAY_INVERT);
}

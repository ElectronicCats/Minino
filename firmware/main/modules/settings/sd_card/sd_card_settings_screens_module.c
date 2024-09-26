#include "oled_screen.h"

#ifdef CONFIG_RESOLUTION_128X64
  #define screen_64 1
#else  // CONFIG_RESOLUTION_128X32
  #define screen_64 0
#endif

void sd_card_settings_screens_module_no_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("No SD Card", screen_64 ? 2 : 0,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("detected!", screen_64 ? 3 : 1,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", screen_64 ? 5 : 3, OLED_DISPLAY_INVERT);
}

void sd_card_settings_screens_module_sd_card_ok() {
  oled_screen_clear();
  oled_screen_display_text_center("SD Card can", screen_64 ? 2 : 0,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("be used safety", screen_64 ? 3 : 1,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", screen_64 ? 5 : 3, OLED_DISPLAY_INVERT);
}

void sd_card_settings_screens_module_wrong_format() {
  oled_screen_clear();
  oled_screen_display_text_center("SD Card is not", screen_64 ? 1 : 0,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("in FAT32,", screen_64 ? 2 : 1,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Format?", screen_64 ? 3 : 2,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", screen_64 ? 5 : 3, OLED_DISPLAY_INVERT);
}

void sd_card_settings_screens_module_format_question() {
  oled_screen_clear();
  oled_screen_display_text_center("SD Card data", screen_64 ? 1 : 0,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("will be lost!", screen_64 ? 2 : 1,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Format?", screen_64 ? 3 : 2,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", screen_64 ? 5 : 3, OLED_DISPLAY_INVERT);
}

void sd_card_settings_screens_module_formating_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("Formating SD", screen_64 ? 2 : 0,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Card... don't", screen_64 ? 3 : 1,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("remove it!", screen_64 ? 4 : 2,
                                  OLED_DISPLAY_NORMAL);
}

void sd_card_settings_screens_module_failed_format_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("Failed to", screen_64 ? 2 : 0,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("format SD Card", screen_64 ? 3 : 1,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", screen_64 ? 5 : 3, OLED_DISPLAY_INVERT);
}

void sd_card_settings_screens_module_format_done() {
  oled_screen_clear();
  oled_screen_display_text_center("Format done!", screen_64 ? 3 : 1,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", screen_64 ? 5 : 3, OLED_DISPLAY_INVERT);
}

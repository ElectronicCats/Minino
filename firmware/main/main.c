#include <stdio.h>

#include "buzzer.h"
#include "cat_console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "flash_fs.h"
#include "flash_fs_screens.h"
#include "general_flash_storage.h"
#include "keyboard_module.h"
#include "leds.h"
#include "menus_module.h"
#include "preferences.h"
#include "sd_card.h"
#include "uart_bridge.h"

#define BAUD_RATE        115200
#define UART_BUFFER_SIZE 1024
#define BUZZER_PIN       GPIO_NUM_2

static const char* TAG = "main";
void app_main() {
  // #if !defined(CONFIG_MAIN_DEBUG)
  //   esp_log_level_set(TAG, ESP_LOG_NONE);
  // #endif

  uart_config_t uart_config = {
      .baud_rate = BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };

  uart_bridge_begin(uart_config, UART_BUFFER_SIZE);
  preferences_begin();
  logs_output_set_output(preferences_get_uchar("logs_output", USB));

  bool stealth_mode = preferences_get_bool("stealth_mode", false);
  if (!stealth_mode) {
    buzzer_enable();
    leds_begin();
  }
  buzzer_begin(BUZZER_PIN);
  sd_card_begin();
  flash_fs_begin(flash_fs_screens_handler);
  keyboard_module_begin();
  menus_module_begin();
  leds_off();
  preferences_put_bool("wifi_connected", false);
  char* values[1] = {
      "Never gonna give you upNever gonna let you downNever gonna run around "
      "and desert youNever gonna make you cryNever gonna say goodbyeNever "
      "gonna tell a lie and hurt youWe've known each other for so longYour "
      "heart's been aching, but you're too shy to say iInside, we both know "
      "what's been going onWe know the game and we're gonna play itAnd if you "
      "ask me how I'm feelingDon't tell me you're too blind to seeNever gonna "
      "give you upNever gonna let you downNever gonna run around and desert "
      "youNever gonna make you cryNever gonna say goodbyeNever gonna tell a "
      "lie and hurt youNever gonna give you upNever gonna let you downNever "
      "gonna run around and desert youNever gonna make you cryNever gonna say "
      "goodbyeNever gonna tell a lie and hurt you"};
  storage_contex_t new_ssid;
  new_ssid.main_storage_name = "spam";
  new_ssid.item_storage_name = "Nevergonna";
  new_ssid.items_storage_value = malloc(UART_BUFFER_SIZE);
  strcpy(new_ssid.items_storage_value, values);
  flash_storage_save_list_items(&new_ssid);
  new_ssid.main_storage_name = "spam";
  new_ssid.item_storage_name = "sextape";
  new_ssid.items_storage_value = values;
  flash_storage_save_list_items(&new_ssid);
  new_ssid.main_storage_name = "wifi";
  new_ssid.item_storage_name = "Hacknet_EXT";
  new_ssid.items_storage_value = values;
  flash_storage_save_list_items(&new_ssid);
  flash_storage_show_list("spam");
  flash_storage_show_list("wifi");
  // esp_err_t err = flash_storage_save_str_item("ssid", "Never gonna give you
  // upNever gonna let you downNever gonna run around and desert youNever gonna
  // make you cryNever gonna say goodbyeNever gonna tell a lie and hurt youWe've
  // known each other for so longYour heart's been aching, but you're too shy to
  // say iInside, we both know what's been going onWe know the game and we're
  // gonna play itAnd if you ask me how I'm feelingDon't tell me you're too
  // blind to seeNever gonna give you upNever gonna let you downNever gonna run
  // around and desert youNever gonna make you cryNever gonna say goodbyeNever
  // gonna tell a lie and hurt youNever gonna give you upNever gonna let you
  // downNever gonna run around and desert youNever gonna make you cryNever
  // gonna say goodbyeNever gonna tell a lie and hurt you"); if(err != ESP_OK){
  //   ESP_LOGW("main", "Error");
  // }
  // ESP_LOGI(TAG, "Saved");
  // char* str_ssid = malloc(1024);
  // err = flash_storage_get_str_item("ssid", str_ssid);
  // ESP_LOGI(TAG, "%s", str_ssid);
  // free(str_ssid);
  cat_console_begin();  // Contains a while(true) loop, it must be at the end
}

#include "argtable3/argtable3.h"
#include "cat_console.h"
#include "cmd_control.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "general_flash_storage.h"
#include "menus_module.h"

static const char* TAG = "spamssid_cmd";

static struct {
  struct arg_str* name;
  struct arg_str* value;
  struct arg_end* end;
} ssdi_args;

static int save_ssid(int argc, char** argv);

static int save_ssid(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &ssdi_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, ssdi_args.end, argv[0]);
    return 1;
  }
  storage_contex_t new_ssid;
  new_ssid.main_storage_name = GENFLASH_STORAGE_SPAM;
  new_ssid.item_storage_name = ssdi_args.name->sval[0];
  new_ssid.items_storage_value = malloc(GENFLASH_STORAGE_MAX_LEN_STR);
  strcpy(new_ssid.items_storage_value, ssdi_args.value->sval[0]);
  flash_storage_save_list_items(&new_ssid);
  return 0;
}

void cmd_spamssid_register_commands() {
  ssdi_args.name = arg_str0(NULL, "name", "<n>",
                            "Name of the SSID spam (Max 17 characteres)");
  ssdi_args.value = arg_str1(NULL, NULL, "<value>",
                             "Words of the spam comma separed for each SSID");
  ssdi_args.end = arg_end(2);

  const esp_console_cmd_t save_cmd = {
      .command = "spam_save",
      .help = "Save SSID for spam",
      .category = "spam",
      .hint = NULL,
      .func = &save_ssid,
      .argtable = &ssdi_args,
  };

  ESP_ERROR_CHECK(esp_console_cmd_register(&save_cmd));
}
#include "argtable3/argtable3.h"
#include "cat_console.h"
#include "cmd_control.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "general_flash_storage.h"
#include "menus_module.h"

static struct {
  struct arg_str* name;
  struct arg_str* value;
  struct arg_end* end;
} ssdi_save_args;

static struct {
  struct arg_str* idx;
  struct arg_end* end;
} ssdi_delete_args;

static int save_ssid_cmd(int argc, char** argv);
static int delete_ssid_cmd(int argc, char** argv);
static int show_ssid_cmd(int argc, char** argv);
static void show_ssid();

static char* spam_ssids_list[99] = {};
static uint8_t list_count = 0;

static void get_ssid() {
  storage_contex_t list[99];

  flash_storage_get_list(GENFLASH_STORAGE_SPAM, list, &list_count);
  for (int i = 0; i < list_count; i++) {
    spam_ssids_list[i] = list[i].item_storage_name;
  }
}

static void show_ssid() {
  get_ssid();

  flash_storage_show_list(GENFLASH_STORAGE_SPAM);
}

static int show_ssid_cmd(int argc, char** argv) {
  show_ssid();
  return 0;
}

static int delete_ssid_cmd(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &ssdi_delete_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, ssdi_delete_args.end, argv[0]);
    return 1;
  }
  int spam_idx = atoi(ssdi_delete_args.idx->sval[0]);
  get_ssid();
  if (spam_idx < 0 || spam_idx >= (int) list_count) {
    ESP_LOGW(__func__, "Index out of range");
    return 1;
  }
  flash_storage_delete_list_item(GENFLASH_STORAGE_SPAM,
                                 spam_ssids_list[spam_idx]);
  show_ssid();
  return 0;
}

static int save_ssid_cmd(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &ssdi_save_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, ssdi_save_args.end, argv[0]);
    return 1;
  }
  if (strlen(ssdi_save_args.name->sval[0]) > 17) {
    ESP_LOGE(__func__, "SSID name character limit exceded.");
    printf("[ERROR] SSID name characters limit exceded");
    return 1;
  }
  storage_contex_t new_ssid;
  new_ssid.main_storage_name = GENFLASH_STORAGE_SPAM;
  new_ssid.item_storage_name = ssdi_save_args.name->sval[0];
  new_ssid.items_storage_value = malloc(GENFLASH_STORAGE_MAX_LEN_STR);
  strcpy(new_ssid.items_storage_value, ssdi_save_args.value->sval[0]);
  flash_storage_save_list_items(&new_ssid);
  return 0;
}

void cmd_spamssid_register_commands() {
  ssdi_save_args.name =
      arg_str0(NULL, "name", "<n>",
               "Name of the SSID spam (Max 17 characteres, NOT USE SPACES)");
  ssdi_save_args.value = arg_str1(
      NULL, NULL, "<value>", "Words of the spam comma separed for each SSID");
  ssdi_save_args.end = arg_end(2);

  ssdi_delete_args.idx = arg_str1(NULL, NULL, "<idx>", "Index of the SSID");
  ssdi_delete_args.end = arg_end(1);

  esp_console_cmd_t spamssid_save_cmd = {
      .command = "spam_save",
      .help = "Save SSID for spam",
      .category = GENFLASH_STORAGE_SPAM,
      .hint = NULL,
      .func = &save_ssid_cmd,
      .argtable = &ssdi_save_args,
  };
  esp_console_cmd_t spamssid_delete_cmd = {
      .command = "spam_delete",
      .help = "Delete SSID for spam",
      .category = GENFLASH_STORAGE_SPAM,
      .hint = NULL,
      .func = &delete_ssid_cmd,
      .argtable = &ssdi_delete_args,
  };
  esp_console_cmd_t spamssid_show_cmd = {
      .command = "spam_show",
      .help = "Show SSID's list",
      .category = GENFLASH_STORAGE_SPAM,
      .hint = NULL,
      .func = &show_ssid_cmd,
      .argtable = NULL,
  };

  ESP_ERROR_CHECK(esp_console_cmd_register(&spamssid_delete_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&spamssid_save_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&spamssid_show_cmd));
}
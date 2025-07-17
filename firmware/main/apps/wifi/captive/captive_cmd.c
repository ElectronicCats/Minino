#include "captive_cmd.h"
#include <string.h>
#include "argtable3/argtable3.h"
#include "captive_module.h"
#include "cat_console.h"
#include "esp_console.h"
#include "esp_log.h"

static struct {
  struct arg_str* name;
  struct arg_end* end;
} captivecmd_ap_name_args;

static int captivecmd_change_name(int argc, char** argv) {
  int nerros = arg_parse(argc, argv, (void**) &captivecmd_ap_name_args);
  if (nerros != 0) {
    arg_print_errors(stderr, captivecmd_ap_name_args.end, "CAPTIVE_NAME");
    return 1;
  }

  if (strlen(captivecmd_ap_name_args.name->sval[0]) > CAPTIVE_PORTAL_MAX_NAME) {
    printf("Error:  Name must be 100 characters or fewer.\n");
    return 1;
  }

  captive_module_change_ap_name(captivecmd_ap_name_args.name->sval[0]);

  return 0;
}

void captivecmd_register_cmd() {
  captivecmd_ap_name_args.name =
      arg_str0(NULL, NULL, "String", "Captive Portal Name");
  captivecmd_ap_name_args.end = arg_end(1);

  esp_console_cmd_t captivecmd_name = {
      .command = "captive_name",
      .category = "captive",
      .hint = NULL,
      .help =
          "Change the name of the Captive Portal Access Point. Name must be "
          "100 characters or fewer and no spaces.",
      .func = &captivecmd_change_name,
      .argtable = &captivecmd_ap_name_args};
  ESP_ERROR_CHECK(esp_console_cmd_register(&captivecmd_name));
}
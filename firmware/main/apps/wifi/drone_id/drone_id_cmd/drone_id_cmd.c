#include "drone_id_cmd.h"

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "drone_id_preferences.h"

static struct {
  struct arg_dbl* latitude;
  struct arg_dbl* longitude;
  struct arg_end* end;
} drone_location_args;

static int set_drone_location(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &drone_location_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, drone_location_args.end, "set_drone_location");
    return 1;
  }

  drone_id_preferences_set_latitude(*drone_location_args.latitude->dval);
  drone_id_preferences_set_longitude(*drone_location_args.longitude->dval);

  return 0;
}

void drone_id_cmd_register_location_cmd() {
  drone_location_args.latitude =
      arg_dbl1(NULL, NULL, "<latitude>", "Latitude coordinate");
  drone_location_args.longitude =
      arg_dbl1(NULL, NULL, "<longitude>", "Longitude coordinate");
  drone_location_args.end = arg_end(2);

  esp_console_cmd_t drone_set_location_cmd = {
      .command = "drone_set_location",
      .help =
          "\nSet drone location Example: drone_set_location -- 37.7749 "
          "-122.4194\nIt's important to use '--' to avoid parsing errors\n",
      .category = "Drone",
      .hint = NULL,
      .func = &set_drone_location,
      .argtable = &drone_location_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&drone_set_location_cmd));
}

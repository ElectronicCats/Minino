#include <stdlib.h>
#include <string.h>
#include "cmd_control.h"
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_sdr_module.h"
#include "esdr_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard_module.h"
#include "app_manager.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "rf_spectrum.h"

static const char* TAG = "cmd_control";

static struct {
  struct arg_str* app;
  struct arg_end* end;
} message_args;

#define LAUNCH_TASK_STACK 6144
#define LAUNCH_MAX_LEN 32

typedef struct {
  char entry_cmd[LAUNCH_MAX_LEN];
} launch_args_t;

static void launch_app_task(void* arg) {
  launch_args_t* args = (launch_args_t*) arg;
  menus_module_set_menu_over_cmd(args->entry_cmd);
  free(args);
  vTaskDelete(NULL);
}

static int launch_app(int argc, char** argv) {
  if (argc < 2 || argv == NULL || argv[0] == NULL) {
    printf("Usage: launch <app>\n");
    return 1;
  }
  int nerrors = arg_parse(argc, argv, (void**) &message_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, message_args.end, argv[0]);
    return 1;
  }
  if (message_args.app->count != 1 || message_args.app->sval[0] == NULL ||
      strlen(message_args.app->sval[0]) == 0) {
    printf("Error: App name required\n");
    return 1;
  }
  if (strlen(message_args.app->sval[0]) >= LAUNCH_MAX_LEN) {
    printf("Error: app name too long (max %d chars)\n", LAUNCH_MAX_LEN - 1);
    return 1;
  }
  /* message_args.app->sval[0] points into argtable3's own buffer, which the
   * console task reuses on the next command. Copy it before handing it to
   * another task. */
  launch_args_t* args = malloc(sizeof(launch_args_t));
  if (args == NULL) {
    ESP_LOGE(TAG, "Out of memory launching app");
    return 1;
  }
  strncpy(args->entry_cmd, message_args.app->sval[0], sizeof(args->entry_cmd) - 1);
  args->entry_cmd[sizeof(args->entry_cmd) - 1] = '\0';

  BaseType_t task_ret =
      xTaskCreate(launch_app_task, "launch_app_task", LAUNCH_TASK_STACK, args,
                  15, NULL);
  if (task_ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to launch app task");
    free(args);
    return 1;
  }
  return 0;
}

/*
 * Injects a synthetic button event through the real input pipeline.
 *
 * The screensaver wake-up, the menu render and every app's input handler are
 * only reachable through a button press, and that is exactly the path that
 * regressed (the animation used to overwrite the menu frame after the button
 * task drew it). Having a way to drive it from the console makes the behaviour
 * checkable on a board with no buttons pressed.
 */
static int press_cmd(int argc, char** argv) {
  static const char* kButtons[] = {"boot", "left", "right", "up", "down"};
  if (argc < 2) {
    printf("Usage: press <boot|left|right|up|down> [event]\n");
    printf("  event: press_down (default), press_up, long_press_hold, "
           "single_click, double_click\n");
    return 1;
  }

  int button = -1;
  for (unsigned i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); i++) {
    if (!strcmp(argv[1], kButtons[i])) {
      button = (int) i;
      break;
    }
  }
  if (button < 0) {
    printf("Unknown button '%s'\n", argv[1]);
    return 1;
  }

  uint8_t event = BUTTON_PRESS_DOWN;
  if (argc >= 3) {
    if (!strcmp(argv[2], "press_up")) {
      event = BUTTON_PRESS_UP;
    } else if (!strcmp(argv[2], "long_press_hold")) {
      event = BUTTON_LONG_PRESS_HOLD;
    } else if (!strcmp(argv[2], "single_click")) {
      event = BUTTON_SINGLE_CLICK;
    } else if (!strcmp(argv[2], "double_click")) {
      event = BUTTON_DOUBLE_CLICK;
    } else if (!strcmp(argv[2], "press_down")) {
      event = BUTTON_PRESS_DOWN;
    } else {
      printf("Unknown event '%s'\n", argv[2]);
      return 1;
    }
  }

  if (!keyboard_module_inject_event((uint8_t) button, event)) {
    printf("Could not queue the event (input queue not running?)\n");
    return 1;
  }
  printf("Injected %s %s\n", argv[1], argv[2] ? argv[2] : "press_down");
  return 0;
}

static int exit_app_cmd(int argc, char** argv) {
  (void)argc;
  (void)argv;
  if (app_manager_is_active()) {
    printf("Exiting current application...\n");
    app_manager_exit();
  } else if (menus_module_get_app_state()) {
    printf("Exiting current application...\n");
    menus_module_exit_app();
  } else {
    printf("Not currently inside an application.\n");
  }
  return 0;
}

/*
 * Reports how much is actually drawn on the panel, and redraws on demand.
 *
 * "Screen is black" is ambiguous between a dead panel and an empty frame
 * buffer, and the two have very different causes. This makes the distinction
 * objective: after a button press the count must be well above zero, and
 * `display redraw` forces a fresh render of the current menu.
 */
static int display_cmd(int argc, char** argv) {
  if (argc >= 2 && !strcmp(argv[1], "redraw")) {
    menus_module_refresh();
    vTaskDelay(pdMS_TO_TICKS(120));
  }
  size_t ink = oled_screen_ink();
  printf("oled ink: %u bytes, menu %d, in_app %d\n", (unsigned) ink,
         (int) menus_module_get_current_menu(),
         (int) menus_module_get_app_state());
  if (ink == 0) {
    printf("  -> the frame buffer is EMPTY (panel alive, nothing drawn)\n");
  } else {
    printf("  -> something is drawn\n");
  }
  return 0;
}

void cmd_control_register_display_cmd() {
  const esp_console_cmd_t display = {
      .command = "display",
      .help = "Report OLED frame buffer ink; 'display redraw' re-renders",
      .hint = "[redraw]",
      .argtable = NULL,
      .func = &display_cmd,
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&display));
}

void cmd_control_register_press_cmd() {
  const esp_console_cmd_t press = {
      .command = "press",
      .help = "Inject a synthetic button event (test hook)",
      .hint = "<boot|left|right|up|down> [event]",
      .argtable = NULL,
      .func = &press_cmd,
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&press));
}

void cmd_control_register_launch_cmd() {
  message_args.app = arg_str1(NULL, NULL, "<app>",
                              "Name of the app to LAUNCH\n"
                              "analyzer\n"
                              "captive\n"
                              "deauth\n"
                              "deauth_scan\n"
                              "dos\n"
                              "ssid_spam\n"
                              "drone_id\n"
                              "droneid_scanner\n"
                              "modbus_tcp\n"
                              "flock\n"
                              "flock_sim\n"
                              "spam\n"
                              "spam_apple\n"
                              "spam_android\n"
                              "spam_windows\n"
                              "spam_samsung\n"
                              "spam_all\n"
                              "hid\n"
                              "trackers\n"
                              "switch\n"
                              "light\n"
                              "zigbee_sniffer\n"
                              "broadcast\n"
                              "thread_sniffer\n"
                              "gps_wardriving\n"
                              "i2c_scanner\n"
                              "uart_bridge\n"
                              "sdr\n"
                              "spectrum\n"
                              "file_manager\n"
                              "ota"

                              "\n\n"
                              "Keep this list in sync with the entry_cmd fields in\n"
                              "menus_data.c; menus_module_set_menu_over_cmd()\n"
                              "matches them exactly.\n\n"
                              "Example: launch analyzer\n"
                              "Example: launch i2c_scanner");
  message_args.end = arg_end(2);

  esp_console_cmd_t launch_cmd = {.command = "launch",
                                  .help = "Launch an app",
                                  .category = "Minino",
                                  .hint = NULL,
                                  .func = &launch_app,
                                  .argtable = &message_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&launch_cmd));

  esp_console_cmd_t exit_cmd = {.command = "exit_app",
                                .help = "Exit currently running application and return to menu",
                                .category = "Minino",
                                .hint = NULL,
                                .func = &exit_app_cmd,
                                .argtable = NULL};
  ESP_ERROR_CHECK(esp_console_cmd_register(&exit_cmd));
}

static void sdr_capture_task(void* arg) {
  (void) arg;
  /* Through the menu, like the button path: the menu owns the exit callback
     of the app, and a command that starts the app on its own leaves it
     running forever (the next sdr_capture would find it already up). */
  menus_module_set_menu_over_cmd("sdr");
  vTaskDelete(NULL);
}

static int sdr_capture(int argc, char** argv) {
  (void) argv;
  if (argc != 1) {
    printf("Usage: sdr_capture\n");
    return 1;
  }
  BaseType_t task_ret =
      xTaskCreate(sdr_capture_task, "sdr_capture_task", 4096, NULL, 15, NULL);
  if (task_ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to start sdr_capture task");
    return 1;
  }
  return 0;
}

/* The ESP-SDR browser viewer (https://espargos.net/espsdr/app/) talks to the
   board over the same USB port the console uses, so the port has to change
   hands. This command runs inside the console task, which means the console
   is between two reads when the session starts: the port has exactly one
   reader and the bytes cannot be split between two of them. */
static int sdr_host(int argc, char** argv) {
  (void) argv;
  if (argc != 1) {
    printf("Usage: sdr_host\n");
    return 1;
  }

  printf("Serving the ESP-SDR protocol on the USB port.\n");
  printf("Open https://espargos.net/espsdr/app/ and connect to this board.\n");
  printf("The console comes back when the viewer sends RELEASE, or type MININO.\n");
  fflush(stdout);

  esp_err_t err = esdr_host_run();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "The SDR host session ended with an error: %s",
             esp_err_to_name(err));
    return 1;
  }
  return 0;
}

void cmd_control_register_sdr_cmd() {
  esp_console_cmd_t sdr_capture_cmd = {.command = "sdr_capture",
                                       .help =
                                           "Open the native SDR I/Q capture "
                                           "app (RIGHT captures, LEFT exits)",
                                       .category = "Minino",
                                       .hint = NULL,
                                       .func = &sdr_capture,
                                       .argtable = NULL};

  ESP_ERROR_CHECK(esp_console_cmd_register(&sdr_capture_cmd));

  esp_console_cmd_t sdr_host_cmd = {.command = "sdr_host",
                                    .help =
                                        "Serve the ESP-SDR burst protocol on "
                                        "the USB port for the ESP-WebSDR "
                                        "browser viewer (RELEASE or MININO "
                                        "to go back)",
                                    .category = "Minino",
                                    .hint = NULL,
                                    .func = &sdr_host,
                                    .argtable = NULL};

  ESP_ERROR_CHECK(esp_console_cmd_register(&sdr_host_cmd));
}



static int threshold_cmd_func(int argc, char** argv) {
  if (argc == 1) {
    int8_t th = rf_spectrum_get_threshold();
    if (th <= RF_NOISE_FLOOR) {
      printf("Current spectrum threshold: OFF (showing all signals)\n");
    } else {
      printf("Current spectrum threshold: %d dBm\n", th);
    }
    return 0;
  }

  if (strcmp(argv[1], "off") == 0) {
    rf_spectrum_set_threshold(RF_NOISE_FLOOR);
    printf("Spectrum threshold disabled (OFF)\n");
    return 0;
  }

  int val = atoi(argv[1]);
  if (val > 0) val = -val; /* Auto-negate if typed e.g. 75 instead of -75 */
  if (val < -95 || val > -20) {
    printf("Error: Threshold must be between -95 and -20 dBm (or 'off')\n");
    return 1;
  }

  rf_spectrum_set_threshold((int8_t)val);
  printf("Spectrum threshold set to %d dBm\n", val);
  return 0;
}

static int spectrum_audio_cmd_func(int argc, char** argv) {
  if (argc == 1) {
    bool en = rf_spectrum_get_audio_enabled();
    printf("RF Spectrum audio Geiger: %s\n", en ? "ENABLED" : "MUTED");
    return 0;
  }
  if (strcmp(argv[1], "on") == 0) {
    rf_spectrum_set_audio_enabled(true);
    printf("RF Spectrum audio Geiger: ENABLED\n");
  } else if (strcmp(argv[1], "off") == 0) {
    rf_spectrum_set_audio_enabled(false);
    printf("RF Spectrum audio Geiger: MUTED\n");
  } else if (strcmp(argv[1], "toggle") == 0) {
    rf_spectrum_toggle_audio();
    printf("RF Spectrum audio Geiger: %s\n", rf_spectrum_get_audio_enabled() ? "ENABLED" : "MUTED");
  } else {
    printf("Usage: spectrum_audio [on|off|toggle]\n");
    return 1;
  }
  return 0;
}

static int spectrum_log_cmd_func(int argc, char** argv) {
  if (argc == 1) {
    bool en = rf_spectrum_get_sd_log_enabled();
    printf("RF Spectrum SD Logging: %s (target: /sdcard/spectrum_log.csv)\n", en ? "RECORDING" : "IDLE");
    return 0;
  }
  if (strcmp(argv[1], "on") == 0) {
    rf_spectrum_set_sd_log_enabled(true);
    printf("RF Spectrum SD Logging: STARTED\n");
  } else if (strcmp(argv[1], "off") == 0) {
    rf_spectrum_set_sd_log_enabled(false);
    printf("RF Spectrum SD Logging: STOPPED\n");
  } else if (strcmp(argv[1], "toggle") == 0) {
    rf_spectrum_toggle_sd_log();
    printf("RF Spectrum SD Logging: %s\n", rf_spectrum_get_sd_log_enabled() ? "STARTED" : "STOPPED");
  } else {
    printf("Usage: spectrum_log [on|off|toggle]\n");
    return 1;
  }
  return 0;
}

static int spectrum_band_cmd_func(int argc, char** argv) {
  if (argc == 1) {
    rf_protocol_t p = rf_spectrum_get_protocol();
    printf("Current RF band: %s\n", (p == RF_PROTOCOL_ZIGBEE) ? "Zigbee / 802.15.4" : "Wi-Fi 2.4 GHz");
    return 0;
  }
  if (strcmp(argv[1], "wifi") == 0) {
    rf_spectrum_set_protocol(RF_PROTOCOL_WIFI);
    printf("RF Band set to: Wi-Fi 2.4 GHz (Channels 1-14)\n");
  } else if (strcmp(argv[1], "zigbee") == 0) {
    rf_spectrum_set_protocol(RF_PROTOCOL_ZIGBEE);
    printf("RF Band set to: Zigbee / IEEE 802.15.4 (Channels 11-26)\n");
  } else {
    printf("Usage: spectrum_band [wifi|zigbee]\n");
    return 1;
  }
  return 0;
}

static int spectrum_ids_cmd_func(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "clear") == 0) {
    rf_spectrum_clear_alerts();
    printf("RF IDS alerts and counters cleared.\n");
    return 0;
  }

  printf("=== Minino RF IDS Status ===\n");
  uint16_t total_deauth = rf_spectrum_get_deauth_count(0);
  printf("Total Deauth/Disassoc Frames Detected: %u\n", total_deauth);
  for (uint8_t c = 1; c <= 14; c++) {
    uint16_t cnt = rf_spectrum_get_deauth_count(c);
    bool jam = rf_spectrum_has_jammer_alert(c);
    if (cnt > 0 || jam) {
      printf("  CH %2d: %u deauths %s\n", c, cnt, jam ? "[JAMMER DETECTED!]" : "");
    }
  }
  return 0;
}

static int spectrum_pcap_cmd_func(int argc, char** argv) {
  if (argc == 1) {
    bool en = rf_spectrum_get_pcap_enabled();
    printf("RF Spectrum PCAP Capture: %s\n", en ? "RECORDING" : "IDLE");
    return 0;
  }
  if (strcmp(argv[1], "on") == 0) {
    rf_spectrum_set_pcap_enabled(true);
  } else if (strcmp(argv[1], "off") == 0) {
    rf_spectrum_set_pcap_enabled(false);
  } else if (strcmp(argv[1], "toggle") == 0) {
    rf_spectrum_toggle_pcap();
  } else {
    printf("Usage: spectrum_pcap [on|off|toggle]\n");
    return 1;
  }
  printf("RF Spectrum PCAP Capture: %s\n", rf_spectrum_get_pcap_enabled() ? "STARTED" : "STOPPED");
  return 0;
}

static int spectrum_drone_cmd_func(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "clear") == 0) {
    rf_spectrum_clear_alerts();
    printf("Drone alerts and counters cleared.\n");
    return 0;
  }

  printf("=== Minino DroneID / Remote ID Status ===\n");
  uint16_t total_drones = rf_spectrum_get_drone_count(0);
  printf("Total DroneID Frames Detected: %u\n", total_drones);
  const char* last_id = rf_spectrum_get_last_drone_id();
  if (last_id && last_id[0] != '\0') {
    printf("Last Detected UAS ID: %s\n", last_id);
  }
  for (uint8_t c = 1; c <= 14; c++) {
    uint16_t cnt = rf_spectrum_get_drone_count(c);
    if (cnt > 0) {
      printf("  CH %2d: %u DroneID frames\n", c, cnt);
    }
  }
  return 0;
}

void cmd_control_register_threshold_cmd(void) {
  esp_console_cmd_t th_cmd = {.command = "threshold",
                              .help = "Set or view RF Spectrum RSSI threshold filter (dBm or 'off')",
                              .category = "RF",
                              .hint = "[<dBm|off>]",
                              .func = &threshold_cmd_func,
                              .argtable = NULL};

  esp_console_cmd_t aud_cmd = {.command = "spectrum_audio",
                               .help = "Control RF Spectrum audio Geiger feedback (on, off, toggle)",
                               .category = "RF",
                               .hint = "[<on|off|toggle>]",
                               .func = &spectrum_audio_cmd_func,
                               .argtable = NULL};

  esp_console_cmd_t log_cmd = {.command = "spectrum_log",
                               .help = "Control RF Spectrum MicroSD CSV logging (on, off, toggle)",
                               .category = "RF",
                               .hint = "[<on|off|toggle>]",
                               .func = &spectrum_log_cmd_func,
                               .argtable = NULL};

  esp_console_cmd_t pcap_cmd = {.command = "spectrum_pcap",
                                .help = "Control RF Spectrum standard PCAP capture to MicroSD (on, off, toggle)",
                                .category = "RF",
                                .hint = "[<on|off|toggle>]",
                                .func = &spectrum_pcap_cmd_func,
                                .argtable = NULL};

  esp_console_cmd_t band_cmd = {.command = "spectrum_band",
                                .help = "Switch RF Spectrum band (wifi, zigbee)",
                                .category = "RF",
                                .hint = "[<wifi|zigbee>]",
                                .func = &spectrum_band_cmd_func,
                                .argtable = NULL};

  esp_console_cmd_t ids_cmd = {.command = "spectrum_ids",
                               .help = "View or clear RF IDS deauth & jammer alerts (clear)",
                               .category = "RF",
                               .hint = "[clear]",
                               .func = &spectrum_ids_cmd_func,
                               .argtable = NULL};

  esp_console_cmd_t drone_cmd = {.command = "spectrum_drone",
                                 .help = "View or clear DroneID / Remote ID detections (clear)",
                                 .category = "RF",
                                 .hint = "[clear]",
                                 .func = &spectrum_drone_cmd_func,
                                 .argtable = NULL};

  ESP_ERROR_CHECK(esp_console_cmd_register(&th_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&aud_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&log_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&pcap_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&band_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&ids_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&drone_cmd));
}




// ESP libaries
#include <string.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"

// Header for this file
#include "hello_cmd.h"
// Custom actions with screen
// Get the general bitmaps with the minino letters
#include "bitmaps_general.h"
// Get the oled library
#include "oled_screen.h"
// Get the screen saver to deactivate
#include "screen_saver.h"

// Struct for the arguments of the command
static struct {
  struct arg_str* name;
  struct arg_end* end;
} hello_cmd_args;

/* This functions allows handle the command arguments */
static int hello_cmd_handler(int argc, char** argv) {
  /* Check for error in the command params*/
  int nerrros = arg_parse(argc, argv, (void**) &hello_cmd_args);
  if (nerrros != 0) {
    arg_print_errors(stderr, hello_cmd_args.end, "HELLO CMD");
    return 1;
  }

  /* Stop the screen saver. We do this manually in this example,
  but if you use a function from generals you dont need to do manually,
  this is only when you want complete control of what you are doing*/
  screen_saver_stop();

  // Clear the oled
  oled_screen_clear();
  /* Show the minino letters in the center of the screen, the bitmap struct of
  general are: const epd_bitmap_t minino_letters_bitmap = { .idx =
  MININO_LETTERS, // The index of the element .name = "Letters", // The name to
  show in the menus .bitmap = epd_bitmap_minino_text_logo, // The bitmap array
    .width = 64, // The width of the bitmap
    .height = 32, // Height of the bitmap
  };
  */
  oled_screen_display_bitmap(minino_letters_bitmap.bitmap, 32, 16,
                             minino_letters_bitmap.width,
                             minino_letters_bitmap.height, OLED_DISPLAY_NORMAL);
  // Show the user input in the center of the screen at position 4
  oled_screen_display_text_center("Of:", 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(hello_cmd_args.name->sval[0], 6,
                                  OLED_DISPLAY_INVERT);

  printf("\nMeow %s! Says Minino\n", hello_cmd_args.name->sval[0]);

  return 0;
}

/* This function is used to register the commands in the main command register
 in the file: modules/cat_dos/cat_console
*/
void hello_cmd_register() {
  /* Setup the arg name, with this we declare the "n" and "name" as a option
  that we can use as: hello_cmd name=MyName ; hello_cmd n=MyName With tis params
  the user is obligated to declare the n or name as part of the argument, if you
  want just to put the command and the arg, for example for single argument, set
  to NULL the shortopts and longopts like: arg_str0(NULL, NULL, "Str", "Your
  name");
  */
  hello_cmd_args.name = arg_str0("n", "name", "Str", "Your name");
  /* Just set the amount of argument we accept*/
  hello_cmd_args.end = arg_end(1);

  // We init the console command
  esp_console_cmd_t hello_cmd_setup = {
      .command = "hello_cmd",               // This is how we call it
      .help = "Im a simple command hello",  // Help to the user to know about
                                            // what the command does
      .category = "CMD",  // If the app module have an category, this used to
                          // group commands
      .hint = NULL,  // Some help to the user, this requires interactive console
      .func = &hello_cmd_handler,   // Pointer to the callback function that
                                    // handle our values
      .argtable = &hello_cmd_args,  // Pointer to our struct of argument
  };

  /* ESP_ERROR_CHECK function check if the return value of a function it is
  different of ESP_OK then show and error an reboot. If everything it is ok,
  then cointinue the normal process With esp_console_cmd_register we register
  our console cmd to use*/
  ESP_ERROR_CHECK(esp_console_cmd_register(&hello_cmd_setup));
}
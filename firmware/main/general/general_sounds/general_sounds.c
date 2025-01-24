#include "general_sounds.h"
#include "buzzer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void general_sound_play(general_sounds_t sound) {
  switch (sound) {
    case PLAY_PULSE:
      buzzer_play_for(keyboard_sound.time);
      break;
    default:
      break;
  }
}
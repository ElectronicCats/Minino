#pragma once

#include "entities.h"

typedef enum { PLAY_PULSE, PLAY_COUNT } general_sounds_t;

sound_context_t keyboard_sound = {.count = 1, .time = 2};

void general_sound_play(general_sounds_t sound);
#pragma once

#include "stdio.h"

typedef enum {
  ANALYZER_MAIN_SCENE,
  ANALYZER_RUN_SCENE,
  ANALYZER_SETTINGS_SCENE,
  ANALYZER_DESTINATION_SCENE,
  ANALYZER_CHANNEL_SCENE,
  ANALYZER_HELP_SCENE,
} analyzer_scenes_e;

void analyzer_scenes_main_menu();
void analyzer_scenes_settings();
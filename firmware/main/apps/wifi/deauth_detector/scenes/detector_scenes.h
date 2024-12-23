#pragma once

#include <stdio.h>

void detector_scenes_main_menu();
void detector_scenes_settings();
void detector_scenes_channel();
void detector_scenes_help();

void detector_scenes_show_table(uint16_t* deauth_packets_count_list);
void detector_scenes_show_count(uint16_t count, uint8_t channel);
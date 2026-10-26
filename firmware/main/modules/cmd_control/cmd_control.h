#pragma once

#include <stdio.h>

void cmd_control_register_launch_cmd();
void cmd_control_register_press_cmd();
void cmd_control_register_display_cmd();

void cmd_control_register_sdr_cmd();

void cmd_control_register_uart_bridge_commands();

void cmd_control_register_system_commands();

void cmd_control_register_threshold_cmd();

void cmd_spamssid_register_commands();
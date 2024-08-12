#pragma once

#include <stdbool.h>
#include <stdio.h>

void ble_hid_begin();
void ble_hid_volume_down(bool press);
void ble_hid_volume_up(bool press);
#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef void (*hid_event_callback_f)(bool connection);

void ble_hid_begin();
void ble_hid_volume_down(bool press);
void ble_hid_volume_up(bool press);
void ble_hid_play(bool press);
void ble_hid_pause(bool press);
void ble_hid_get_device_name(char* device_name);
void ble_hid_get_device_mac(uint8_t* mac);
void ble_hid_register_callback(hid_event_callback_f callback);
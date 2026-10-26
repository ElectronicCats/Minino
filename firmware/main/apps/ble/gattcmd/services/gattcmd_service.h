#ifndef __GATTCMD_ENUM_H
#define __GATTCMD_ENUM_H

#include <stdint.h>

#define GATTCMD_ENUM_TAG "GATT_ENUM"
/* Size of each service's single-entry profile table. */
#define GATTCMD_ENUM_PROFILE 1
/* Profile table index of each service's single profile instance (there is
   one entry per service table). */
#define GATTCMD_PROFILE_IDX 0
#define INVALID_HANDLE 0

/*
 * Unique Bluedroid GATTC app_ids. esp_ble_gattc_app_register() rejects
 * duplicates, and two services sharing app_id 0 (this file used to define it
 * and bt_gattc.c uses the same value for DEVICE_PROFILE) collided: the second
 * registration failed outright or confused the single global gattc_if -> app
 * demux. bt_gattc owns 0, the gatt console services take 2...5.
 */
#define GATTCMD_ENUM_APP_ID  2
#define GATTCMD_SCAN_APP_ID  3
#define GATTCMD_RECON_APP_ID 4
#define GATTCMD_WRITE_APP_ID 5

void gattcmd_enum_begin(char* saddress);
void gattcmd_write_begin(char* saddress, uint16_t target_uuid, char* value_str);
void gattcmd_scan_begin();
void gattcmd_recon_begin();

void gattcmd_scan_stop();
void gattcmd_enum_stop();
void gattcmd_write_stop();
void gattcmd_recon_stop();

#endif

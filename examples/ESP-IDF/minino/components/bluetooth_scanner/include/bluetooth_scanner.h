#ifndef BLUETOOTH_SCANNER_H
#define BLUETOOTH_SCANNER_H

typedef struct {
    uint8_t mac[6];
    int rssi;
    const char* name;
    bool is_airtag;
    uint16_t count;
    bool has_finished;
} bluetooth_scanner_record_t;

typedef void (*bluetooth_scanner_cb_t)(bluetooth_scanner_record_t record);

void bluetooth_scanner_init();
void bluetooth_scanner_register_cb(bluetooth_scanner_cb_t cb);
void bluetooth_scanner_start();
void bluetooth_scanner_stop();
void bluetooth_scanner_deinit();
bool bluetooth_scanner_is_active();

#endif
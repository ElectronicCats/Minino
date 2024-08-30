
#ifndef LED_EVENTS_H
#define LED_EVENTS_H

typedef void (*effect_control)(void);

void led_control_run_effect(effect_control effect_function);
void led_control_stop(void);
void led_control_pulse_leds(void);
void led_control_pulse_led_right(void);
void led_control_pulse_led_left(void);
// BLE
void led_control_ble_tracking(void);
void led_control_ble_spam_breathing(void);

// WIFI
void led_control_wifi_scanning(void);
void led_control_wifi_attacking(void);

// Zigbee
void led_control_zigbee_scanning(void);
#endif  // LED_EVENTS_H

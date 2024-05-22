
#ifndef LED_EVENTS_H
#define LED_EVENTS_H

#define GPIO_LED_RED   2
#define GPIO_LED_GREEN 3
#define GPIO_LED_BLUE  10

#define RED    0xFF0000
#define GREEN  0x00FF00
#define BLUE   0x0000FF
#define YELLOW 0xFFa500
#define VIOLET 0xEE82EE

typedef void (*effect_coontrol)(void);

void led_control_run_effect(effect_coontrol effect_function);
void led_control_begin(void);
void led_control_stop(void);
// Game
void led_control_game_event_pairing(void);
void led_control_game_event_blue_team_turn(void);
void led_control_game_event_red_team_turn(void);
void led_control_game_event_attacking(void);
void led_control_game_event_red_team_winner(void);
void led_control_game_event_blue_team_winner(void);
// BLE
void led_control_ble_tracking(void);
void led_control_ble_spam_breathing(void);

// WIFI
void led_control_wifi_scanning(void);
void led_control_wifi_attacking(void);

// Zigbee
void led_control_zigbee_scanning(void);
#endif  // LED_EVENTS_H

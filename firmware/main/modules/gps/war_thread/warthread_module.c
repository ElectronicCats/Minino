#include "warthread_module.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "general_submenu.h"
#include "gps_module.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "radio_selector.h"
#include "sd_card.h"
#include "wardriving_common.h"
#include "wardriving_screens_module.h"
#include "open_thread.h"
#include "radio_selector.h"


#define FILE_NAME WARTH_DIR_NAME "/WarThread"

static const char* TAG = "warthread";

static void warthread_packet_handler(const otRadioFrame* aFrame, bool aIsTx){
  ESP_LOGI(TAG, "Packet received");
  otLogHexDumpInfo info;

  info.mDataBytes = aFrame->mPsdu;
  info.mDataLength = aFrame->mLength;
  info.mTitle = "New Packet";
  info.mIterator = 0;

  printf("\n");

  while (otLogGenerateNextHexDumpLine(&info) == OT_ERROR_NONE) {
    printf("%s\n", info.mLine);
  }
}

void warthread_module_begin() {
  radio_selector_set_thread();
  openthread_init();
  openthread_set_dataset(11, 0x1234);
  openthread_enable_promiscous_mode(&warthread_packet_handler);
}
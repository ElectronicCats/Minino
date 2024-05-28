#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_vfs_eventfd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/uart_types.h"
#include "nvs_flash.h"
#include "openthread/dataset.h"
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/logging.h"
#include "openthread/message.h"
#include "openthread/tasklet.h"
#include "openthread/thread.h"
#include "openthread/udp.h"
#include "openthread_config.h"
#include "sdkconfig.h"

#include "oled_screen.h"
#include "openthread.h"
#include "preferences.h"

#if CONFIG_OPENTHREAD_FTD
  #include "openthread/dataset_ftd.h"
#endif

#define ERR error != OT_ERROR_NONE
#define TAG "Openthread"

#define THREAD_CHANNEL           15
#define THREAD_PANID             0x1234
#define THREAD_NETWORK_NAME      "OpenThread-ESP"
#define THREAD_EXTPANID          "dead00beef00cafe"
#define THREAD_MESH_LOCAL_PREFIX "fd00:db8:a0:0::/64"
#define THREAD_NETWORK_MASTERKEY "00112233445566778899aabbccddeeff"
#define THREAD_NETWORK_PSKC      "104810e2315100afd6bc9215a6bfac53"

otUdpSocket mSocket;
otIp6Address mAddr;
TaskHandle_t sender_task;
static esp_netif_t* init_openthread_netif(
    const esp_openthread_platform_config_t* config) {
  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
  esp_netif_t* netif = esp_netif_new(&cfg);
  assert(netif != NULL);
  ESP_ERROR_CHECK(
      esp_netif_attach(netif, esp_openthread_netif_glue_init(config)));

  return netif;
}

static int hex_digit_to_int(char hex) {
  if ('A' <= hex && hex <= 'F') {
    return 10 + hex - 'A';
  }
  if ('a' <= hex && hex <= 'f') {
    return 10 + hex - 'a';
  }
  if ('0' <= hex && hex <= '9') {
    return hex - '0';
  }
  return -1;
}

static size_t hex_string_to_binary(const char* hex_string,
                                   uint8_t* buf,
                                   size_t buf_size) {
  int num_char = strlen(hex_string);

  if (num_char != buf_size * 2) {
    return 0;
  }
  for (size_t i = 0; i < num_char; i += 2) {
    int digit0 = hex_digit_to_int(hex_string[i]);
    int digit1 = hex_digit_to_int(hex_string[i + 1]);

    if (digit0 < 0 || digit1 < 0) {
      return 0;
    }
    buf[i / 2] = (digit0 << 4) + digit1;
  }

  return buf_size;
}

void openthread_factory_reset() {
  preferences_put_bool("thread_deinit", true);
  otInstanceFactoryReset(esp_openthread_get_instance());
}

esp_err_t set_dataset() {
  otInstance* instance = esp_openthread_get_instance();
  printf("BUILDING DATSET\n");
  otOperationalDataset dataset;
  size_t len = 0;
#if CONFIG_OPENTHREAD_FTD
  otDatasetCreateNewNetwork(instance, &dataset);
#else
  memset(&dataset, 0, sizeof(otOperationalDataset));
#endif
  // Active timestamp
  dataset.mActiveTimestamp.mSeconds = 1;
  dataset.mActiveTimestamp.mTicks = 0;
  dataset.mActiveTimestamp.mAuthoritative = false;
  dataset.mComponents.mIsActiveTimestampPresent = true;

  // Channel, Pan ID, Network Name
  dataset.mChannel = THREAD_CHANNEL;
  dataset.mComponents.mIsChannelPresent = true;
  dataset.mPanId = THREAD_PANID;
  dataset.mComponents.mIsPanIdPresent = true;
  len = strlen(THREAD_NETWORK_NAME);
  assert(len <= OT_NETWORK_NAME_MAX_SIZE);
  memcpy(dataset.mNetworkName.m8, THREAD_NETWORK_NAME, len + 1);
  dataset.mComponents.mIsNetworkNamePresent = true;

  // Extended Pan ID
  len = hex_string_to_binary(THREAD_EXTPANID, dataset.mExtendedPanId.m8,
                             sizeof(dataset.mExtendedPanId.m8));
  if (len != sizeof(dataset.mExtendedPanId.m8))
    ESP_LOGE(TAG, "Cannot convert OpenThread extended pan id");
  dataset.mComponents.mIsExtendedPanIdPresent = true;

  // Mesh Local Prefix
  otIp6Prefix prefix;
  memset(&prefix, 0, sizeof(otIp6Prefix));
  if (otIp6PrefixFromString(THREAD_MESH_LOCAL_PREFIX, &prefix) ==
      OT_ERROR_NONE) {
    memcpy(dataset.mMeshLocalPrefix.m8, prefix.mPrefix.mFields.m8,
           sizeof(dataset.mMeshLocalPrefix.m8));
    dataset.mComponents.mIsMeshLocalPrefixPresent = true;
  } else {
    ESP_LOGE("Falied to parse mesh local prefix", THREAD_MESH_LOCAL_PREFIX);
  }

  // Network Key
  len = hex_string_to_binary(THREAD_NETWORK_MASTERKEY, dataset.mNetworkKey.m8,
                             sizeof(dataset.mNetworkKey.m8));
  if (len != sizeof(dataset.mNetworkKey.m8))
    ESP_LOGE(TAG, "Cannot convert OpenThread master key");
  dataset.mComponents.mIsNetworkKeyPresent = true;

  // PSKc
  len = hex_string_to_binary(THREAD_NETWORK_PSKC, dataset.mPskc.m8,
                             sizeof(dataset.mPskc.m8));
  if (len != sizeof(dataset.mPskc.m8))
    ESP_LOGE(TAG, "Cannot convert OpenThread pre-shared commissioner key");
  dataset.mComponents.mIsPskcPresent = true;

  if (otDatasetSetActive(instance, &dataset) != OT_ERROR_NONE)
    ESP_LOGE(TAG, "Failed to set OpenThread active dataset");

  return ESP_OK;
}

esp_err_t thread_init(bool init) {
  esp_err_t error;
  otInstance* instance = esp_openthread_get_instance();

  error = otIp6SetEnabled(instance, init);
  error = otThreadSetEnabled(instance, init);

  if (ERR)
    ESP_LOGE("Thread Set Failed", "");
  else
    ESP_LOGI("Thread Set Success", "");

  return error;
}

void print_otIp6Address(const otIp6Address* direccion) {
  char ip[OT_IP6_ADDRESS_STRING_SIZE];
  otIp6AddressToString(direccion, ip, OT_IP6_ADDRESS_STRING_SIZE);
  printf("IP: %s\n", ip);
}

void print_data() {
  ESP_LOGI("INFO", "");
  otError error = OT_ERROR_NONE;
  otOperationalDatasetTlvs dataset;
  otInstance* instance = esp_openthread_get_instance();

  const otNetifAddress* unicastAddrs = otIp6GetUnicastAddresses(instance);
  mAddr = unicastAddrs->mNext->mAddress;
  print_otIp6Address(&mAddr);

  error = otDatasetGetActiveTlvs(instance, &dataset);
  if (!ERR) {
    printf("DATASET: ");
    for (uint8_t i = 0; i < dataset.mLength; i++)
      printf("%02x", dataset.mTlvs[i]);
    printf("\n");
  } else
    ESP_LOGE("ERROR", "");
}

void on_udp_recieve(void* aContext,
                    otMessage* aMessage,
                    const otMessageInfo* aMessageInfo) {
  otError error = OT_ERROR_NONE;
  printf("NUEVO MENSAJE\n");
  oled_screen_clear();
  oled_screen_display_text("New message...  ", 0, 0, OLED_DISPLAY_INVERT);
  char buf[1500];
  int length;
  char ip[60];

  ESP_LOGI(TAG, "%d bytes from ",
           otMessageGetLength(aMessage) - otMessageGetOffset(aMessage));

  char src_addr[OT_IP6_ADDRESS_STRING_SIZE];
  otIp6AddressToString(&aMessageInfo->mPeerAddr, src_addr, sizeof(src_addr));
  ESP_LOGI(TAG, "SRC: %s", src_addr);
  oled_screen_display_text("SRC ADDRESS", 0, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(src_addr, 0, 5, OLED_DISPLAY_NORMAL);
  otIp6AddressToString(&aMessageInfo->mSockAddr, src_addr, sizeof(src_addr));
  ESP_LOGI(TAG, "DST: %s", src_addr);

  ESP_LOGI(TAG, "PORT: %d", aMessageInfo->mPeerPort);
  sprintf(ip, "PORT: %d", aMessageInfo->mPeerPort);
  oled_screen_display_text(ip, 0, 2, OLED_DISPLAY_NORMAL);

  length = otMessageRead(aMessage, otMessageGetOffset(aMessage), buf,
                         sizeof(buf) - 1);
  if (length >= 0) {
    buf[length] = '\0';
    printf("%s\n", buf);
    oled_screen_display_text(buf, 20, 7, OLED_DISPLAY_NORMAL);
  } else {
    ESP_LOGE(TAG, "Error al leer el mensaje");
    oled_screen_display_text("Error!!!", 25, 7, OLED_DISPLAY_NORMAL);
  }
}

otError UDP_bind(uint16_t port) {
  otError error = OT_ERROR_NONE;
  otSockAddr sockaddr;

  memset(&sockaddr.mAddress, 0, sizeof(otIp6Address));
  sockaddr.mPort = port;
  error = otUdpBind(esp_openthread_get_instance(), &mSocket, &sockaddr,
                    OT_NETIF_THREAD);

  return error;
}

otError UDP_open() {
  ESP_LOGI(TAG, "Socket INIT");
  otError error;

  if (otUdpIsOpen(esp_openthread_get_instance(), &mSocket))
    error = OT_ERROR_ALREADY;
  else
    error = otUdpOpen(esp_openthread_get_instance(), &mSocket, on_udp_recieve,
                      NULL);
  return error;
}
otError UDP_close() {
  otError error = OT_ERROR_NONE;
  error = otUdpClose(esp_openthread_get_instance(), &mSocket);
  if (ERR)
    ESP_LOGE("", "UDP CLOSE FAILED");
  else
    ESP_LOGI("", "UDP CLOSE OK");
  return error;
}

otError UDP_send(const char* msg, const char* dst, uint16_t port) {
  esp_openthread_lock_acquire(portMAX_DELAY);

  otError error = OT_ERROR_NONE;
  otMessage* message = NULL;
  otMessageInfo messageInfo;
  otMessageSettings messageSettings = {true, OT_MESSAGE_PRIORITY_NORMAL};

  memset(&messageInfo, 0, sizeof(messageInfo));

  otIp6AddressFromString(dst, &messageInfo.mPeerAddr);
  print_otIp6Address(&messageInfo.mPeerAddr);
  messageInfo.mPeerPort = port;
  printf("PORT: %d\n", messageInfo.mPeerPort);

  message = otUdpNewMessage(esp_openthread_get_instance(), &messageSettings);
  if (message == NULL)
    printf("ERR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  size_t msg_len = strlen(msg);
  otMessageAppend(message, msg, msg_len);

  error =
      otUdpSend(esp_openthread_get_instance(), &mSocket, message, &messageInfo);

  if (ERR)
    ESP_LOGE("Send ERR", "");
  else
    ESP_LOGI("Send OK", "");
  esp_openthread_lock_release();
  return error;
}

static void udp_sender() {
  otError error = OT_ERROR_NONE;
  while (true) {
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    ESP_LOGI("SENDING...", "");
    error = UDP_send("HOLAAAAAAAA", "ff02::1", 2222);
  }
  printf("SENDER FINISHED\n");
}

static void ot_task_worker() {
  esp_openthread_platform_config_t config = {
      .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
      .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
      .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
  };

  // Initialize the OpenThread stack
  ESP_ERROR_CHECK(esp_openthread_init(&config));

  // Initialize the esp_netif bindings
  esp_netif_t* openthread_netif;
  openthread_netif = init_openthread_netif(&config);
  esp_netif_set_default_netif(openthread_netif);

  set_dataset(NULL);
  thread_init(true);

  print_data();
  UDP_open();
  UDP_bind(2222);

  esp_openthread_launch_mainloop();
  printf("LOOP FINISHED\n");
  vTaskDelete(sender_task);

  // Clean up
  esp_openthread_netif_glue_deinit();
  esp_netif_destroy(openthread_netif);

  esp_vfs_eventfd_unregister();
  vTaskDelete(NULL);
}
void open_thread_deinit() {
  UDP_close();
  thread_init(false);
  esp_openthread_deinit();
}

void openthread_init(void) {
  esp_vfs_eventfd_config_t eventfd_config = {
      .max_fds = 3,
  };

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
  xTaskCreate(ot_task_worker, "openthread_main", 1024 * 10, NULL, 10, NULL);
  xTaskCreate(udp_sender, "Sender", 1024 * 5, NULL, 10, &sender_task);
}

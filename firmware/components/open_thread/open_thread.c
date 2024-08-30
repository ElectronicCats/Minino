#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "driver/uart.h"
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
#include "sdkconfig.h"

#include "open_thread.h"
#include "open_thread_config.h"

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

otIp6Address mAddr;
otOperationalDataset dataset;

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

esp_err_t openthread_set_dataset(uint8_t channel, uint16_t panid) {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();
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
  dataset.mChannel = channel;
  dataset.mComponents.mIsChannelPresent = true;
  dataset.mPanId = panid;
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

  otIp6SetEnabled(instance, true);
  otThreadSetEnabled(instance, true);
  esp_openthread_lock_release();
  return ESP_OK;
}

void print_otIp6Address(const otIp6Address* direccion) {
  char ip[OT_IP6_ADDRESS_STRING_SIZE];
  otIp6AddressToString(direccion, ip, OT_IP6_ADDRESS_STRING_SIZE);
  printf("IP: %s\n", ip);
}

otIp6Address openthread_get_my_ipv6address() {
  return mAddr;
}

void openthread_factory_reset() {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstanceFactoryReset(esp_openthread_get_instance());
}

otError openthread_ipmaddr_subscribe(const char* address) {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();
  otError error = OT_ERROR_NONE;
  otIp6Address addr;
  otIp6AddressFromString(address, &addr);
  error = otIp6SubscribeMulticastAddress(instance, &addr);
  esp_openthread_lock_release();
  return error;
}

otError openthread_ipmaddr_unsubscribe(const char* address) {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();
  otError error = OT_ERROR_NONE;
  otIp6Address addr;
  otIp6AddressFromString(address, &addr);
  error = otIp6UnsubscribeMulticastAddress(instance, &addr);
  esp_openthread_lock_release();
  return error;
}

void print_data() {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();
  ESP_LOGI("INFO", "");
  otError error = OT_ERROR_NONE;
  otOperationalDatasetTlvs dataset;

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
  esp_openthread_lock_release();
}

otError openthread_udp_open(otUdpSocket* mSocket, otUdpReceive otUdp_cb) {
  printf("udp open\n");
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();
  otError error;
  if (otUdpIsOpen(instance, mSocket))
    error = OT_ERROR_ALREADY;
  else
    error = otUdpOpen(instance, mSocket, otUdp_cb, NULL);
  esp_openthread_lock_release();
  if (ERR) {
    printf("ERR");
  }
  return error;
}

otError openthread_udp_bind(otUdpSocket* mSocket, uint16_t port) {
  printf("udp bind\n");
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();
  otError error = OT_ERROR_NONE;
  otSockAddr sockaddr;

  memset(&sockaddr.mAddress, 0, sizeof(otIp6Address));
  sockaddr.mPort = port;
  error = otUdpBind(instance, mSocket, &sockaddr, OT_NETIF_THREAD);
  esp_openthread_lock_release();
  return error;
}

otError openthread_udp_close(otUdpSocket* mSocket) {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();
  otError error = OT_ERROR_NONE;
  error = otUdpClose(instance, mSocket);
  esp_openthread_lock_release();
  return error;
}

otError openthread_udp_send(otUdpSocket* mSocket,
                            const char* dst,
                            uint16_t port,
                            void* data,
                            size_t data_size) {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();

  otError error = OT_ERROR_NONE;
  otMessage* message = NULL;
  otMessageInfo messageInfo;
  otMessageSettings messageSettings = {true, OT_MESSAGE_PRIORITY_HIGH};

  memset(&messageInfo, 0, sizeof(messageInfo));

  otIp6AddressFromString(dst, &messageInfo.mPeerAddr);
  messageInfo.mPeerPort = port;

  message = otUdpNewMessage(instance, &messageSettings);

  uint8_t* payload = (uint8_t*) malloc(data_size);
  if (!payload) {
    ESP_LOGE(TAG, "Failed to allocate memory for data");
    return error;
  }
  memcpy(payload, data, data_size);
  otMessageAppend(message, payload, data_size);

  error = otUdpSend(instance, mSocket, message, &messageInfo);
  esp_openthread_lock_release();
  return error;
}

otError openthread_enable_promiscous_mode(otLinkPcapCallback promiscuous_cb) {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();
  otError error = OT_ERROR_NONE;
  otIp6SetEnabled(instance, false);
  otThreadSetEnabled(instance, false);
  error = otLinkSetPromiscuous(instance, true);
  if (ERR) {
    printf("ERR\n");
    return error;
  }
  otLinkSetPcapCallback(instance, promiscuous_cb, NULL);
  otIp6SetEnabled(instance, true);
  otThreadSetEnabled(instance, true);
  esp_openthread_lock_release();
  return error;
}
otError openthread_disable_promiscous_mode() {
  esp_openthread_lock_acquire(portMAX_DELAY);
  otInstance* instance = esp_openthread_get_instance();
  otError error = OT_ERROR_NONE;
  otIp6SetEnabled(instance, false);
  otThreadSetEnabled(instance, false);
  otLinkSetPcapCallback(instance, NULL, NULL);
  error = otLinkSetPromiscuous(instance, false);
  otIp6SetEnabled(instance, true);
  otThreadSetEnabled(instance, true);
  esp_openthread_lock_release();
  return error;
}

static void ot_task_worker() {
  esp_openthread_platform_config_t config = {
      .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
      .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
      .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
  };

  ESP_ERROR_CHECK(esp_openthread_init(&config));
  esp_netif_t* openthread_netif;
  openthread_netif = init_openthread_netif(&config);
  esp_netif_set_default_netif(openthread_netif);

  openthread_set_dataset(THREAD_CHANNEL, THREAD_PANID);
  print_data();

  esp_openthread_launch_mainloop();

  // Clean up
  esp_openthread_netif_glue_deinit();
  esp_netif_destroy(openthread_netif);

  esp_vfs_eventfd_unregister();
  vTaskDelete(NULL);
}

void openthread_deinit() {
  esp_openthread_deinit();
}

void openthread_init() {
#if !defined(CONFIG_OPEN_THREAD_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  esp_vfs_eventfd_config_t eventfd_config = {
      .max_fds = 3,
  };

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
  xTaskCreate(ot_task_worker, "ot_cli_main", 1024 * 5, NULL, 10, NULL);
}

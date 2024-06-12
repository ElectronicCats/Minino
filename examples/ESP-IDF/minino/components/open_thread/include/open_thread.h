#pragma once
#include "esp_err.h"
#include "openthread/dataset.h"
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/logging.h"
#include "openthread/message.h"
#include "openthread/tasklet.h"
#include "openthread/thread.h"
#include "openthread/udp.h"

void openthread_init();

void openthread_deinit();

esp_err_t openthread_set_dataset(uint8_t channel, uint16_t panid);

otError openthread_udp_open(otUdpSocket* mSocket, otUdpReceive ot_recieve_cb);

otError openthread_udp_bind(otUdpSocket* mSocket, uint16_t port);

otError openthread_udp_close(otUdpSocket* mSocket);

otError openthread_udp_send(otUdpSocket* mSocket,
                            const char* dst,
                            uint16_t port,
                            void* data,
                            size_t data_size);

otIp6Address openthread_get_my_ipv6address();

otError openthread_ipmaddr_subscribe(const char* address);

otError openthread_ipmaddr_unsubscribe(const char* address);

void openthread_factory_reset();

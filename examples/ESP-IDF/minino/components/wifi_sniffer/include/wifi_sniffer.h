#ifndef SIMPLE_SNIFFER_H
#define SIMPLE_SNIFFER_H
#define CHANNEL_MAX 13  // US = 11, EU = 13, JP = 14

void wifi_sniffer_init();
void wifi_sniffer_start();
void wifi_sniffer_stop();

#endif  // SIMPLE_SNIFFER_H
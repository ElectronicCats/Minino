#ifndef SIMPLE_SNIFFER_H
#define SIMPLE_SNIFFER_H
#define CHANNEL_MAX 13  // US = 11, EU = 13, JP = 14

void wifi_sniffer_start();
void wifi_sniffer_stop();
void wifi_sniffer_exit();

#endif  // SIMPLE_SNIFFER_H

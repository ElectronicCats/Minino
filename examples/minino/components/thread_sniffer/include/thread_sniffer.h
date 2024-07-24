#pragma once
#include "open_thread.h"

void thread_sniffer_init();
void thread_sniffer_run();
void thread_sniffer_stop();
void thread_sniffer_set_on_link_pcap_cb(otLinkPcapCallback cb);

# Host tests for the BLE spam GAP completion tracker.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# bt_spam_gap_sync.c is the module that decides which GAP completion answers
# which asynchronous request. It is deliberately free of FreeRTOS and ESP-IDF
# dependencies so the desync that broke BLE spam can be reproduced here.

IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g \
          -Imock -I$(SRCDIR) -I$(UNITY)

SRCS := $(SRCDIR)/bt_spam_gap_sync.c \
        test_bt_spam_gap_sync.c \
        $(UNITY)/unity.c

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

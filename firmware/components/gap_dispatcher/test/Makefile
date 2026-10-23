# Host tests for the central GAP event dispatcher.
# SPDX-License-Identifier: GPL-3.0-or-later

IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g \
          -Imock -I$(SRCDIR) -I$(UNITY)

SRCS := $(SRCDIR)/gap_dispatcher.c \
        mock/esp_bt_main.c \
        test_gap_dispatcher.c \
        $(UNITY)/unity.c

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

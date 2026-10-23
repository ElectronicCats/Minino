# Host tests for the RF Spectrum channel-range helpers.
# SPDX-License-Identifier: GPL-3.0-or-later

IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g \
          -I$(SRCDIR) -I$(UNITY)

SRCS := $(SRCDIR)/rf_spectrum_channel.c \
        test_rf_spectrum_channel.c \
        $(UNITY)/unity.c

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

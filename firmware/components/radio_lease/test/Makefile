# Host tests for the shared-radio registry.
# SPDX-License-Identifier: GPL-3.0-or-later

IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g \
          -Imock -I$(SRCDIR)/include -I$(UNITY)

SRCS := $(SRCDIR)/radio_lease_registry.c \
        test_radio_lease_registry.c \
        $(UNITY)/unity.c

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

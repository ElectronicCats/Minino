# Host tests for resistor detector module.
# SPDX-License-Identifier: GPL-3.0-or-later

IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

CFLAGS   := -std=gnu99 -Wall -Wextra -Werror -g -I. -I$(SRCDIR) -I$(UNITY)

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCDIR)/resistor_detector.c test_resistor_detector.c
	@mkdir -p build
	$(CC) $(CFLAGS) $^ $(UNITY)/unity.c -lm -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

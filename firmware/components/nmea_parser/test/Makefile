# SPDX-License-Identifier: Apache-2.0
# Tests de host: gcc contra la Unity vendorizada en ESP-IDF.
IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

SRCS := $(SRCDIR)/nmea_parser.c
TESTS := test_nmea_parser.c

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g \
          -include mock/sdkconfig.h \
          -I. -Imock -I$(SRCDIR)/include -I$(UNITY)

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS) $(TESTS) nmea_test_main.c nmea_test.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) $(TESTS) nmea_test_main.c $(UNITY)/unity.c -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

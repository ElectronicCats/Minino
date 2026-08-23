# SPDX-License-Identifier: GPL-3.0-or-later
# Tests de host: gcc contra la Unity vendorizada en ESP-IDF.
# Requiere IDF_PATH (. $IDF_PATH/export.sh) pero NO usa el build system de IDF.
UNITY  := $(IDF_PATH)/components/unity/unity/src
SRCDIR := ..

SRCS := $(SRCDIR)/surv_signatures.c \
        $(SRCDIR)/surv_match.c \
        $(SRCDIR)/surv_ie.c \
        $(SRCDIR)/surv_overlay.c \
        $(SRCDIR)/surv_engine.c

TESTS := test_surv_signatures.c \
        test_surv_match.c \
        test_surv_ble.c \
        test_surv_ie.c \
        test_surv_engine.c \
        test_surv_overlay.c

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g -I. -I$(SRCDIR)/include -I$(UNITY)

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS) $(TESTS) surv_test_main.c surv_test.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) $(TESTS) surv_test_main.c $(UNITY)/unity.c -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

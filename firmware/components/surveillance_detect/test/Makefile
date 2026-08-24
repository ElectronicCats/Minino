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
        test_surv_ie_diff.c \
        test_surv_engine.c \
        test_surv_overlay.c

# Codigo de terceros para el test diferencial. Se compila aparte y SIN -Werror:
# es un extracto literal de flock-you y no se toca para acallar avisos, porque
# su valor es ser identico al original.
REFSRC := reference/flock_you_ie_reference.c

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g -I. -I$(SRCDIR)/include -I$(UNITY)

run-tests: build/run-tests
	./build/run-tests

build/reference.o: $(REFSRC)
	@mkdir -p build
	$(CC) -std=gnu99 -w -g -c $(REFSRC) -o $@

build/run-tests: $(SRCS) $(TESTS) surv_test_main.c surv_test.h build/reference.o
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) $(TESTS) surv_test_main.c $(UNITY)/unity.c \
	      build/reference.o -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

# Host tests for the task registry.
# SPDX-License-Identifier: GPL-3.0-or-later

IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g \
          -Imock -I$(SRCDIR) -I$(UNITY)

SRCS := $(SRCDIR)/task_manager.c \
        test_task_manager.c \
        $(UNITY)/unity.c

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS) $(wildcard mock/*.h mock/freertos/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

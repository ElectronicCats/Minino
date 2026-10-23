# Host tests for command parsing and validation.
# SPDX-License-Identifier: GPL-3.0-or-later

IDF_PATH  ?= $(HOME)/esp/esp-idf
UNITY     := $(IDF_PATH)/components/unity/unity/src
ARGTABLE3 := $(IDF_PATH)/components/console/argtable3
SRCDIR    := ..
CATDOSDIR := ../../cat_dos

CFLAGS    := -std=gnu99 -Wall -Wextra -Werror -g -fno-strict-aliasing \
             -I. -Imock -I$(SRCDIR) -I$(SRCDIR)/gpio -I$(SRCDIR)/spamssid_cmd \
             -I$(CATDOSDIR) -I../../app_manager -I../../keyboard -I../../oled_screen -I$(UNITY) -I$(IDF_PATH)/components/console -I$(ARGTABLE3)

SRCS      := $(SRCDIR)/cmd_control.c \
             $(SRCDIR)/cmd_system.c \
             $(SRCDIR)/gpio/cmd_uart_bridge.c \
             $(SRCDIR)/spamssid_cmd/spamssid_cmd.c \
             $(CATDOSDIR)/cmd_catdos.c \
             test_cmd_parsing.c \
             app_manager_stub.c \
             $(wildcard $(ARGTABLE3)/*.c) \
             $(UNITY)/unity.c

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -lm -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

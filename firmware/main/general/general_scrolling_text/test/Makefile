# Host tests for the scrolling-text line splitter.
# SPDX-License-Identifier: GPL-3.0-or-later

IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g \
          -I. -I$(SRCDIR) -I$(UNITY)

# Only the splitter: general_scrolling_text.c also pulls in the menu, keyboard
# and OLED layers, which have nothing to do with line wrapping.
SRCS := $(SRCDIR)/general_scroll_split.c \
        test_general_scrolling_text.c \
        $(UNITY)/unity.c

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

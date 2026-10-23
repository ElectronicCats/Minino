# Host tests for the menu catalogue.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The catalogue used to be *defined* in a header (menus_include/menus.h), which
# gave every one of the ~64 translation units that include menus_module.h its
# own 2.3 KB copy. The link only succeeded because
# components/wifi_attacks/CMakeLists.txt passed -Wl,-zmuldefs to the whole
# firmware, which also masked ~70 other duplicate symbols defined in headers.
#
# These tests compile menus_data.c for the host and assert the invariants that
# the linker can no longer enforce for us.
#
# The same sources are compiled twice, once with every feature enabled and once
# with a feature switched off, because the structural bugs only appear in the
# reduced configuration: the MENU_SETTINGS_SD_CARD_* children used to be emitted
# outside #if CONFIG_SD_ENABLED while their parent was inside it, which is
# invisible in a full build and an unrecoverable menu state on a board without
# an SD card.

IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g \
          -I. -Imock -I$(SRCDIR) -I$(SRCDIR)/menus_include -I../../app_manager -I$(UNITY)

# menus_data.c references the on_enter/on_exit callbacks of every app in the
# tree, so linking it for the host requires stubbing them all out. The stubs
# live in menu_callbacks_stub.c and are only ever called through the table,
# never executed.
SRCS := $(SRCDIR)/menus_data.c \
        menu_callbacks_stub.c \
        test_menus_data.c \
        $(UNITY)/unity.c

VARIANTS := full no-sd no-bluetooth no-wifi no-stacks

run-tests: $(addprefix build/run-,$(VARIANTS))
	@set -e; for v in $(VARIANTS); do \
	  echo "--- menus catalogue: $$v ---"; ./build/run-$$v; \
	done

# Every feature enabled: the default board profile.
build/run-full: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

# Reduced configurations. Each one must still satisfy every structural
# invariant: no orphan children, parents before children, unique ids.
build/run-no-sd: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -DTEST_CONFIG_SD_ENABLED=0 $^ -o $@

build/run-no-bluetooth: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -DTEST_CONFIG_BLUETOOTH_APPS_ENABLE=0 $^ -o $@

build/run-no-wifi: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -DTEST_CONFIG_WIFI_APPS_ENABLE=0 $^ -o $@

# The Zigbee and the Thread stacks out: the apps of those stacks are not
# compiled in and their entries have to go from the catalogue with them.
build/run-no-stacks: $(SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -DTEST_CONFIG_MININO_ZIGBEE_STACK=0 \
		-DTEST_CONFIG_MININO_OPENTHREAD_STACK=0 $^ -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

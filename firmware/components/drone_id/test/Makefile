# SPDX-License-Identifier: Apache-2.0
# Tests de host: g++/gcc contra la Unity vendorizada en ESP-IDF.
IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..

C_SRCS := $(SRCDIR)/alt_unix_time/alt_unix_time.c \
          $(SRCDIR)/opendroneid/opendroneid.c \
          $(SRCDIR)/odid_wifi/wifi.c

CPP_SRCS := $(SRCDIR)/utm/utm.cpp \
            $(SRCDIR)/id_open/id_open.cpp \
            $(SRCDIR)/id_open_esp32/id_open_beacon.cpp \
            $(SRCDIR)/spoofer/spoofer.cpp

TESTS := test_utm.cpp \
         test_alt_unix_time.cpp \
         test_opendroneid.cpp \
         test_odid_wifi.cpp \
         test_spoofer.cpp

INCLUDES := -I. -Imock -I$(SRCDIR) \
            -I$(SRCDIR)/utm \
            -I$(SRCDIR)/alt_unix_time \
            -I$(SRCDIR)/opendroneid \
            -I$(SRCDIR)/odid_wifi \
            -I$(SRCDIR)/id_open \
            -I$(SRCDIR)/id_open_esp32 \
            -I$(SRCDIR)/spoofer \
            -I$(UNITY)

CFLAGS   := -std=gnu99 -Wall -Wextra -Werror -g -DUNITY_INCLUDE_DOUBLE $(INCLUDES)
CXXFLAGS := -std=gnu++17 -Wall -Wextra -Werror -g -DUNITY_INCLUDE_DOUBLE $(INCLUDES)

run-tests: build/run-tests
	./build/run-tests

build/c_objs.o: $(C_SRCS) $(UNITY)/unity.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $(SRCDIR)/alt_unix_time/alt_unix_time.c -o build/alt_unix_time.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/opendroneid/opendroneid.c -o build/opendroneid.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/odid_wifi/wifi.c -o build/wifi.o
	$(CC) $(CFLAGS) -c $(UNITY)/unity.c -o build/unity.o
	touch $@

build/run-tests: build/c_objs.o $(CPP_SRCS) $(TESTS) drone_id_test_main.cpp drone_id_test.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(CPP_SRCS) $(TESTS) drone_id_test_main.cpp \
	      build/alt_unix_time.o build/opendroneid.o build/wifi.o build/unity.o -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

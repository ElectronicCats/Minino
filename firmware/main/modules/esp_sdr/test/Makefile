# Host tests for the Osmosdr / I/Q packing components.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Plain gcc against the Unity vendored in ESP-IDF. It needs IDF_PATH exported
# but does not use the IDF build system, and the sources under test must not
# include ESP-IDF headers: that is what guarantees these run without hardware.

IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src
SRCDIR   := ..
# The I/Q packing lives in the vrt_packet component (shared with the VITA 49
# packet code), not next to the port: the firmware must not carry two copies
# of iq_pack/iq_unpack with the same names.
VRTDIR   := ../../../../components/vrt_packet

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g -I. -I$(SRCDIR) -I$(VRTDIR) -I$(UNITY)

# One binary per suite: each has its own Unity main().
run-tests: build/run-osmosdr-port build/run-esdr-burst build/run-esdr-spectrum
	./build/run-osmosdr-port
	./build/run-esdr-burst
	./build/run-esdr-spectrum

build/run-osmosdr-port: $(SRCDIR)/osmosdr_serial.c $(VRTDIR)/iq_pack.c test_osmosdr_port.c
	@mkdir -p build
	$(CC) $(CFLAGS) $^ $(UNITY)/unity.c -o $@

# The burst protocol the ESP-WebSDR browser viewer speaks. It is plain C on
# purpose, so the whole wire format can be checked without hardware.
build/run-esdr-burst: $(SRCDIR)/esdr_burst_proto.c test_esdr_burst_proto.c
	@mkdir -p build
	$(CC) $(CFLAGS) $^ $(UNITY)/unity.c -o $@

# El modo de espectros: la FFT se comprueba contra un DFT directo.
build/run-esdr-spectrum: $(SRCDIR)/esdr_spectrum.c $(SRCDIR)/esdr_burst_proto.c \
                         test_esdr_spectrum.c
	@mkdir -p build
	$(CC) $(CFLAGS) $^ $(UNITY)/unity.c -lm -o $@

clean:
	rm -rf build

.PHONY: run-tests clean
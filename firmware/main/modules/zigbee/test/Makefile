IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g \
          -I. -I.. -I$(UNITY)

SRCS := test_zigbee_mesh_mapper.c \
        $(UNITY)/unity.c

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS) ../zigbee_mesh_mapper.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -rf build

.PHONY: run-tests clean

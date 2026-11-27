IDF_PATH ?= $(HOME)/esp/esp-idf
UNITY    := $(IDF_PATH)/components/unity/unity/src

CC ?= gcc
CFLAGS = -Wall -Wextra -Werror -I../include -I$(UNITY) -g -O0

SRCS = test_files_ops.c ../files_ops.c $(UNITY)/unity.c
TARGET = test_files_ops

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

test: $(TARGET)
	./$(TARGET)

run-tests: test

clean:
	rm -f $(TARGET)

.PHONY: all test run-tests clean

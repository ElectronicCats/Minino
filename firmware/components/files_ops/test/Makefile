CC ?= gcc
CFLAGS = -Wall -Wextra -Werror -I../include -Iunity -g -O0

SRCS = test_files_ops.c ../files_ops.c unity/unity.c
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

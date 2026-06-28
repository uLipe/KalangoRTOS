CC      := cc
TARGET  := thread_unit_test

ROOT    := ../..
STUBINC := include

CFLAGS := \
    -std=c99 \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -I$(STUBINC) \
    -I$(ROOT) \
    -g -O0

SRCS := \
    thread_unit_test.c \
    $(ROOT)/kernel/thread/thread.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	@echo "--- running thread unit tests ---"
	@./$(TARGET) && echo "THREAD UNIT TEST: PASS" || \
	    { echo "THREAD UNIT TEST: FAIL"; exit 1; }

clean:
	rm -f $(TARGET)

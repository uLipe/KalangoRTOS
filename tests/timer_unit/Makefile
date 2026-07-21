CC      := cc
ROOT    := ../..
TARGET  := timer_unit_test
STUBINC := include

CFLAGS := \
	-std=c99 \
	-Wall -Wextra \
	-Wno-unused-parameter \
	-DUL_UNIT_TEST=1 \
	-I$(STUBINC) \
	-I$(ROOT) \
	-g -O0

SRCS := \
	timer_unit_test.c \
	$(ROOT)/kernel/time/timer_wheel.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	@echo "--- running timer unit tests ---"
	@./$(TARGET) && echo "TIMER UNIT TEST: PASS" || \
	    { echo "TIMER UNIT TEST: FAIL"; exit 1; }

clean:
	rm -f $(TARGET)

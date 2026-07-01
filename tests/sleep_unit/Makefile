CC      := cc
ROOT    := ../..
TARGET  := sleep_unit_test
STUBINC := include

CFLAGS := \
    -std=c99 \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -I$(STUBINC) \
    -I$(ROOT) \
    -g -O0

SRCS := \
    sleep_unit_test.c \
    $(ROOT)/kernel/timer/timer.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	@echo "--- running sleep unit tests ---"
	@./$(TARGET) && echo "SLEEP UNIT TEST: PASS" || \
	    { echo "SLEEP UNIT TEST: FAIL"; exit 1; }

clean:
	rm -f $(TARGET)

CC      := cc
ROOT    := ../..
TARGET  := sched_unit_test
STUBINC := include

CFLAGS := \
    -std=c99 \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -I$(STUBINC) \
    -I$(ROOT) \
    -g -O0

SRCS := \
    sched_unit_test.c \
    $(ROOT)/kernel/sched/fifo_rt.c \
    $(ROOT)/kernel/sched/bitmap_rt.c \
    $(ROOT)/kernel/sched/sched.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	@echo "--- running sched unit tests ---"
	@./$(TARGET) && echo "SCHED UNIT TEST: PASS" || \
	    { echo "SCHED UNIT TEST: FAIL"; exit 1; }

clean:
	rm -f $(TARGET)

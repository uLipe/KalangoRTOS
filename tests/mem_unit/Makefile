CC      := cc
ROOT    := ../..
TARGET  := mem_unit_test
STUBINC := include

CFLAGS := \
    -std=c99 \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -I$(STUBINC) \
    -I$(ROOT) \
    -g -O0

SRCS := \
    mem_unit_test.c \
    $(ROOT)/kernel/percpu/klock.c \
    $(ROOT)/kernel/mem/tlsf.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	@echo "--- running mem unit tests ---"
	@./$(TARGET) && echo "MEM UNIT TEST: PASS" || \
	    { echo "MEM UNIT TEST: FAIL"; exit 1; }

clean:
	rm -f $(TARGET)

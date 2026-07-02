CC      := cc
ROOT    := ../..
TARGET  := ipc_unit_test
STUBINC := include

CFLAGS := \
    -std=c99 \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -DUL_UNIT_TEST \
    -I$(STUBINC) \
    -I$(ROOT) \
    -g -O0

SRCS := \
    ipc_unit_test.c \
    $(ROOT)/kernel/ipc/ep.c \
    $(ROOT)/kernel/notif/notif.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	@echo "--- running IPC unit tests ---"
	@./$(TARGET) && echo "IPC UNIT TEST: PASS" || \
	    { echo "IPC UNIT TEST: FAIL"; exit 1; }

clean:
	rm -f $(TARGET)

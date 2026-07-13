CC     := cc
TARGET := atomic_unit_test

CFLAGS := -std=c99 -Wall -Wextra -g -O0

.PHONY: all run clean

all: $(TARGET)

$(TARGET): atomic_unit_test.c
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	@echo "--- running atomic unit tests ---"
	@./$(TARGET)

clean:
	rm -f $(TARGET)

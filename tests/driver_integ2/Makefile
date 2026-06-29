CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := driver_integ2_test.elf

ROOT    := ../..
GEN_INC := /tmp/gen

CFLAGS := \
    -mcpu=tc27xx \
    -ffreestanding \
    -ffunction-sections \
    -fdata-sections \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -I$(ROOT)/include \
    -I$(ROOT)/arch/tricore/include \
    -I$(ROOT) \
    -I$(GEN_INC) \
    -O0 -g

LDFLAGS := \
    -T ../boot/boot_test.ld \
    -mcpu=tc27xx \
    -nostartfiles \
    -Wl,--gc-sections \
    -Wl,--no-warn-rwx-segments

LIBS := -lc -lgcc

KERNEL_SRCS := \
    $(ROOT)/arch/tricore/startup.S \
    $(ROOT)/arch/tricore/arch.c \
    $(ROOT)/arch/tricore/ctx_switch.S \
    $(ROOT)/arch/tricore/vectors.S \
    $(ROOT)/kernel/kernel_main.c \
    $(ROOT)/kernel/printk/ul_printk.c \
    $(ROOT)/kernel/sched/sched.c \
    $(ROOT)/kernel/sched/fifo_rt.c \
    $(ROOT)/kernel/irq/irq.c \
    $(ROOT)/kernel/mem/mem.c \
    $(ROOT)/kernel/mem/phys_alloc.c \
    $(ROOT)/kernel/thread/thread.c \
    $(ROOT)/kernel/ipc/ep.c \
    $(ROOT)/kernel/notif/notif.c \
    $(ROOT)/kernel/syscall/syscall_router.c \
    $(ROOT)/kernel/timer/timer.c \
    $(ROOT)/stub/board_init_stub.c

TEST_SRCS := \
    driver_integ2_test.c \
    $(ROOT)/boards/qemu_tc27x/qemu_console.c

SRCS := $(KERNEL_SRCS) $(TEST_SRCS)

# PASS sentinels for the full hardware path.
# The PARTIAL sentinel is accepted as PASS by the run target below.
SENTINELS := \
    "driver_integ2: start" \
    "driver_integ2: driver ready"

.PHONY: all run clean gen_config

gen_config:
	@mkdir -p $(GEN_INC)/ul
	@printf '#ifndef UL_CONFIG_H\n#define UL_CONFIG_H\n' > $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_THREADS      32\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_ENDPOINTS    16\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_NOTIFS       32\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_IRQ_BINDINGS 16\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_TICK_HZ          1000\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_DEBUG_PRINTK     1\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_SCHED_QUANTUM_US    10000\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_SCHED_QUANTUM_TICKS 10\n' >> $(GEN_INC)/ul/config.h
	@printf '#endif\n' >> $(GEN_INC)/ul/config.h

all: gen_config $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	@echo "--- running driver_integ2 (STM1 hardware timer, timeout=30s) ---"
	@$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>/dev/null | \
	    timeout 30 tee /tmp/driver_integ2_test.log ; \
	if grep -q "driver_integ2: PARTIAL" /tmp/driver_integ2_test.log 2>/dev/null; then \
	    echo "DRIVER INTEG2 TEST: PARTIAL (STM1 not supported by emulator)"; \
	    exit 0; \
	fi; \
	PASS=1; \
	for s in $(SENTINELS) \
	    "driver_integ2: client0 alarm fired at tick" \
	    "driver_integ2: client1 alarm fired at tick" \
	    "driver_integ2: client2 alarm fired at tick" \
	    "driver_integ2: ordering PASS" \
	    "driver_integ2: PASS"; do \
	    if grep -q "$$s" /tmp/driver_integ2_test.log 2>/dev/null; then \
	        echo "  [PASS] found: $$s"; \
	    else \
	        echo "  [FAIL] missing: $$s"; \
	        PASS=0; \
	    fi; \
	done; \
	if [ "$$PASS" = "1" ]; then \
	    echo "DRIVER INTEG2 TEST: PASS"; \
	else \
	    echo "DRIVER INTEG2 TEST: FAIL"; \
	    exit 1; \
	fi

clean:
	rm -f $(TARGET) /tmp/driver_integ2_test.log

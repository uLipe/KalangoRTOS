CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := ipc_integ_test.elf

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
    ipc_integ_test.c \
    $(ROOT)/boards/qemu_tc27x/qemu_console.c

SRCS := $(KERNEL_SRCS) $(TEST_SRCS)

SENTINELS := \
    "ipc_integ: start" \
    "ipc_integ: pp server replied" \
    "ipc_integ: ping-pong PASS" \
    "ipc_integ: mc server done" \
    "ipc_integ: mc client A PASS" \
    "ipc_integ: mc client B PASS" \
    "ipc_integ: mc client C PASS" \
    "ipc_integ: notif producer signalled" \
    "ipc_integ: notif consumer PASS" \
    "ipc_integ: rn server IPC path PASS" \
    "ipc_integ: rn server notif path PASS" \
    "ipc_integ: PASS"

.PHONY: all run clean gen_config

gen_config:
	@mkdir -p $(GEN_INC)/ul
	@printf '#ifndef UL_CONFIG_H\n#define UL_CONFIG_H\n' > $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_THREADS      32\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_ENDPOINTS    16\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_NOTIFS       16\n' >> $(GEN_INC)/ul/config.h
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
	@echo "--- running IPC integration test (timeout=120s) ---"
	@$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>/dev/null | \
	    timeout 120 tee /tmp/ipc_integ_test.log ; \
	PASS=1; \
	for s in $(SENTINELS); do \
	    if grep -q "$$s" /tmp/ipc_integ_test.log 2>/dev/null; then \
	        echo "  [PASS] found: $$s"; \
	    else \
	        echo "  [FAIL] missing: $$s"; \
	        PASS=0; \
	    fi; \
	done; \
	if [ "$$PASS" = "1" ]; then \
	    echo "IPC INTEG TEST: PASS"; \
	else \
	    echo "IPC INTEG TEST: FAIL"; \
	    exit 1; \
	fi

clean:
	rm -f $(TARGET) /tmp/ipc_integ_test.log

# tests/integ_kernel.mk
# Build kernel sources as a static library so boot_test.ld can select
# sections with *libtest_kernel.a:(...) patterns (same model as production).

INTEG_KERNEL_LIB ?= libtest_kernel.a

INTEG_KERNEL_EXTRA_SRCS ?=

# init.c copies .data / zeroes .bss before the C runtime is up — prevent GCC
# from turning those loops into memcpy/memset calls.
INTEG_KERNEL_EXTRA_CFLAGS += -fno-tree-loop-distribute-patterns

# ARCH_KERNEL_SRCS must be set by tests/arch/$(ARCH).mk before include.
INTEG_KERNEL_STUB ?= $(ROOT)/stub/board_init_stub.c

INTEG_KERNEL_SRCS ?= \
	$(ARCH_KERNEL_SRCS) \
	$(ROOT)/kernel/init/init.c \
	$(ROOT)/kernel/kernel_main.c \
	$(ROOT)/kernel/printk/ulmk_printk.c \
	$(ROOT)/kernel/sched/sched.c \
	$(ROOT)/kernel/sched/fifo_rt.c \
	$(ROOT)/kernel/sched/bitmap_rt.c \
	$(ROOT)/kernel/irq/irq.c \
	$(ROOT)/kernel/mem/mem.c \
	$(ROOT)/kernel/mem/tlsf.c \
	$(ROOT)/kernel/thread/thread.c \
	$(ROOT)/kernel/ipc/ep.c \
	$(ROOT)/kernel/notif/notif.c \
	$(ROOT)/kernel/syscall/syscall_router.c \
	$(INTEG_KERNEL_STUB) \
	$(INTEG_KERNEL_EXTRA_SRCS)

INTEG_KERNEL_OBJS := $(patsubst $(ROOT)/%,integ_kernel/%,$(INTEG_KERNEL_SRCS))
INTEG_KERNEL_OBJS := $(INTEG_KERNEL_OBJS:.c=.o)
INTEG_KERNEL_OBJS := $(INTEG_KERNEL_OBJS:.S=.o)

$(INTEG_KERNEL_LIB): $(INTEG_KERNEL_OBJS)
	$(AR) rcs $@ $^

integ_kernel/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INTEG_KERNEL_EXTRA_CFLAGS) -c -o $@ $<

integ_kernel/%.o: $(ROOT)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: integ_kernel_clean integ_kernel_rebuild
integ_kernel_clean:
	rm -rf integ_kernel $(INTEG_KERNEL_LIB)

integ_kernel_rebuild: integ_kernel_clean $(INTEG_KERNEL_LIB)

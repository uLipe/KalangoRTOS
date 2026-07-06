ARCH ?= tricore
ROOT := ../..
TARGET := ctx_switch_test.elf
STRESS := ctx_stress_test.elf

TEST_SRCS := ../test_printk.c ctx_switch_root_thread.c
TEST_EXTRA_SRCS :=

SENTINELS := "switching to root thread" "ROOT THREAD RUNNING"
STRESS_SENTINELS := "ctx_stress: start" "ctx_stress: PASS"

include ../integ_common.mk

STRESS_TEST_SRCS := ctx_switch_stress.c

.PHONY: stress all

all: gen_config $(TARGET) $(STRESS)

$(STRESS): $(INTEG_KERNEL_LIB) $(filter-out ctx_switch_root_thread.c,$(TEST_SRCS)) $(STRESS_TEST_SRCS) $(TEST_LD_PATH)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--whole-archive $(INTEG_KERNEL_LIB) -Wl,--no-whole-archive $(filter-out ctx_switch_root_thread.c,$(TEST_SRCS)) $(STRESS_TEST_SRCS) $(LIBS) -o $@

stress: $(STRESS)
	@echo "--- running ctx_switch stress (ARCH=$(ARCH)) ---"
	@$(TIMEOUT_CMD) $(QEMU) -machine $(MACHINE) $(QEMU_EXTRA) \
	    -kernel $(STRESS) -nographic </dev/null 2>&1 | tee /tmp/$(STRESS:.elf=.log) ; \
	FAIL=0 ; \
	for S in $(STRESS_SENTINELS) ; do \
	    grep -q "$$S" /tmp/$(STRESS:.elf=.log) || FAIL=1 ; \
	done ; \
	[ $$FAIL -eq 0 ] && echo "CTX_STRESS: PASS" || (echo "CTX_STRESS: FAIL"; exit 1)

clean:
	rm -f $(TARGET) $(STRESS) /tmp/ctx_switch_test.log /tmp/ctx_stress_test.log
	$(MAKE) integ_kernel_clean

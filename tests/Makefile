UNIT_TESTS := \
    ipc_unit \
    mem_unit \
    sched_unit \
    sleep_unit \
    thread_unit

INTEG_TESTS := \
    boot \
    ctx_early_tricore \
    ctx_switch \
    atomic_integ \
    ipc_integ \
    irq_integ \
    mem_integ \
    preempt_integ \
    resource_leak_integ \
    sched_integ \
    sleep_integ \
    thread_lifecycle_integ

.PHONY: all unit integ clean

all: unit integ

unit:
	@for t in $(UNIT_TESTS); do \
	    echo "=== $$t ==="; \
	    $(MAKE) -C $$t clean -s 2>/dev/null || true; \
	    $(MAKE) -C $$t run || exit 1; \
	done

integ:
	@for t in $(INTEG_TESTS); do \
	    echo "=== $$t ==="; \
	    $(MAKE) -C $$t clean -s 2>/dev/null || true; \
	    $(MAKE) -C $$t gen_config -s; \
	    $(MAKE) -C $$t run || exit 1; \
	done

clean:
	@for t in $(UNIT_TESTS) $(INTEG_TESTS); do \
	    $(MAKE) -C $$t clean -s 2>/dev/null || true; \
	done

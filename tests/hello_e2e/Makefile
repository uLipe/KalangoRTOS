# Production hello_world smoke test — CMake build + QEMU sentinel check.

QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC397B_TRB
ROOT    := ../..
BUILD_DIR ?= /build/ulipe
KERNEL    := $(BUILD_DIR)/ulmk
LOG       := /tmp/hello_e2e.log
QEMU_TIMEOUT := 30

.PHONY: all build run clean

all: build

build:
	@mkdir -p $(BUILD_DIR)
	cmake -S $(ROOT) -B $(BUILD_DIR) \
	    -DCMAKE_TOOLCHAIN_FILE=$(ROOT)/cmake/toolchain-tricore-gcc.cmake \
	    -DULMK_CHIP_DIR=$(ROOT)/boards/qemu_tc3xx \
	    -GNinja --no-warn-unused-cli
	ninja -C $(BUILD_DIR)
	@test -f $(KERNEL)

run: build
	@echo "--- hello e2e (timeout=$(QEMU_TIMEOUT)s) ---"
	@timeout --kill-after=5 $(QEMU_TIMEOUT) $(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(KERNEL) \
	    -nographic \
	    </dev/null 2>&1 | tee $(LOG) ; \
	FAIL=0 ; \
	if grep -q 'ulmk: switching to root thread' $(LOG) ; then \
	    echo "  [PASS] found: root thread handoff" ; \
	else \
	    echo "  [FAIL] missing: root thread handoff" ; \
	    FAIL=1 ; \
	fi ; \
	for n in 0 1 2 ; do \
	    if grep -q "hello from userspace — tick #$$n" $(LOG) ; then \
	        echo "  [PASS] found: tick #$$n" ; \
	    else \
	        echo "  [FAIL] missing: tick #$$n" ; \
	        FAIL=1 ; \
	    fi ; \
	done ; \
	if [ $$FAIL -eq 0 ]; then \
	    echo "HELLO E2E: PASS" ; \
	else \
	    echo "HELLO E2E: FAIL" ; \
	    cat $(LOG) ; \
	    exit 1 ; \
	fi

clean:
	rm -rf $(BUILD_DIR) $(LOG)

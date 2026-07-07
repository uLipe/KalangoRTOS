# ulmk — Architecture Porting Guide

> **Purpose of this document:** everything needed to port the kernel to a new
> CPU architecture.  Describes which files to create, what each must implement,
> how to wire the build system and linker script, and how to validate the port
> with the existing integration test suite.

---

## Table of Contents

1. [Overview](#1-overview)
2. [New Architecture Checklist](#2-new-architecture-checklist)
3. [Directory and File Layout](#3-directory-and-file-layout)
4. [Step 1 — arch_config.h](#step-1--arch_configh)
5. [Step 2 — ulmk_arch.h (local copy)](#step-2--ulmk_archh-local-copy)
6. [Step 3 — startup.S](#step-3--startups)
7. [Step 4 — vectors.S](#step-4--vectorss)
8. [Step 5 — arch.c (all ulmk_arch_* functions)](#step-5--archc-all-ulmk_arch_-functions)
9. [Step 6 — ctx_switch.S](#step-6--ctx_switchs)
10. [Step 7 — Linker Fragments](#step-7--linker-fragments)
11. [Step 8 — Toolchain CMake File](#step-8--toolchain-cmake-file)
12. [Step 9 — Board Directory](#step-9--board-directory)
13. [Step 10 — Wire CMakeLists.txt](#step-10--wire-cmakeliststxt)
14. [Step 11 — Validate with Integration Tests](#step-11--validate-with-integration-tests)
15. [Common Pitfalls](#15-common-pitfalls)

---

## 1. Overview

The kernel is split into a platform-independent core (`kernel/`) and an arch
port (`arch/<name>/`).  The two halves communicate through a strict interface:

```
kernel/ ──calls──► ulmk_arch_*()        declared in arch/<name>/include/ulmk_arch.h
arch/   ──calls──► ulmk_kernel_*()      declared there too (callbacks from arch to kernel)
```

Nothing in `kernel/` may reference ISA-specific types, registers, or assembly.
Nothing in `arch/<name>/` may include kernel internal headers.

The full contract is documented in `docs/arch_api_spec.md`.

---

## 2. New Architecture Checklist

```
[ ] arch/<name>/include/arch_config.h    constants (MPU region count, etc.)
[ ] arch/<name>/include/ulmk_arch.h        full contract header (types + prototypes)
[ ] arch/<name>/startup.S                _start: stack, context pool, vectors, .data/.bss
[ ] arch/<name>/vectors.S                trap + ISR entry stubs
[ ] arch/<name>/arch.c                   all ulmk_arch_* implementations
[ ] arch/<name>/ctx_switch.S             ulmk_arch_ctx_switch
[ ] arch/<name>/linker/prologue.ld.in    OUTPUT_FORMAT, OUTPUT_ARCH, ENTRY
[ ] arch/<name>/linker/<optional>.ld.in  any arch-specific linker sections
[ ] cmake/toolchain-<name>-<compiler>.cmake
[ ] boards/<board>/board.cmake           ULMK_BOARD_CPU, ULMK_BOARD_CFLAGS, ULMK_BOARD_SOURCES
[ ] boards/<board>/memory.ld             MEMORY block
[ ] boards/<board>/board_services.c      ulmk_board_init + ulmk_printk_char_out + board_services_init
[ ] CMakeLists.txt updated               toolchain file default or documented override
[ ] All 8 integration tests pass
```

---

## 3. Directory and File Layout

```
arch/
└── <name>/
    ├── arch.c               CPU control, MPU, IRQ, syscall/trap entry
    ├── ctx_switch.S         ulmk_arch_ctx_switch (usually assembly)
    ├── startup.S            _start
    ├── vectors.S            trap and ISR handlers
    ├── linker/
    │   ├── prologue.ld.in   OUTPUT_FORMAT, OUTPUT_ARCH, ENTRY(_start)
    │   └── <optional>.ld.in arch-specific sections (register windows, etc.)
    └── include/
        ├── ulmk_arch.h        arch contract (must match docs/arch_api_spec.md)
        └── arch_config.h    arch constants (not referenced by kernel/)
```

---

## Step 1 — arch_config.h

Arch-specific constants referenced only by `arch/<name>/` code.  The kernel
never includes this file.

```c
/* arch/<name>/include/arch_config.h */
#ifndef ULMK_ARCH_CONFIG_H
#define ULMK_ARCH_CONFIG_H

/* Maximum number of MPU regions per protection domain. */
#define ULMK_ARCH_MAX_MPU_REGIONS  8

/* Number of protection domains (sets). */
#define ULMK_ARCH_NUM_PRS          4

/* Stack alignment required by the ABI (bytes). */
#define ULMK_ARCH_STACK_ALIGN      8

/* Syscall instruction and number register. */
#define ULMK_ARCH_SYSCALL_INSN     "svc #0"
#define ULMK_ARCH_SYSCALL_NR_REG   "r7"    /* example for ARM Thumb */

/* ... add whatever your ISA needs ... */

#endif
```

---

## Step 2 — ulmk_arch.h (local copy)

Copy the canonical template from `docs/arch_api_spec.md §3–§12` into
`arch/<name>/include/ulmk_arch.h` and fill in the types:

```c
/* arch/<name>/include/ulmk_arch.h */
#ifndef ULMK_ARCH_H
#define ULMK_ARCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ulmk/microkernel.h>
#include <arch_config.h>

/* ── Arch types ────────────────────────────────────── */

typedef struct {
    uint32_t sp;        /* saved stack pointer (example for ARM) */
    uint32_t pc;        /* (may be implicit via stack frame) */
} ulmk_arch_ctx_t;

typedef uint32_t ulmk_arch_irq_key_t;    /* saved CPSR / DAIF / etc. */

typedef struct {
    uintptr_t base;
    size_t    size;
    uint32_t  perms;
    uint8_t   type;
} ulmk_arch_region_t;

#define ULMK_REGION_CODE   0
#define ULMK_REGION_DATA   1
#define ULMK_REGION_STACK  2
#define ULMK_REGION_HEAP   3
#define ULMK_REGION_PERIPH 4
#define ULMK_REGION_SHARED 5

/* ── Declarations (copy from arch_api_spec.md) ─────── */
/* ulmk_arch_cpu_*, ulmk_arch_ctx_*, ulmk_arch_mpu_*, ulmk_arch_irq_* ... */

#endif
```

Ensure `ulmk_arch_ctx_t` is the **first field** of `ulmk_thread_t` (this is
guaranteed by the kernel's `thread.c` as long as the type is defined here).

---

## Step 3 — startup.S

`_start` is the CPU reset entry point.  It must:

1. Set up the supervisor stack pointer.
2. Initialise any hardware-managed context pool (register windows, CSA, etc.).
3. Program the trap/interrupt vector base register(s).
4. Set the ISR stack pointer.
5. Call `ulmk_board_init()`.
6. Copy `.data` from flash (LMA) to RAM (VMA).
7. Zero `.bss`.
8. Call `ulmk_arch_init(ulmk_boot_info_t *info)` — fills `*info`.
9. Call `ulmk_kern_main(info)` — does not return.

```asm
/* arch/<name>/startup.S — skeleton */
    .section .text._start
    .global  _start
_start:
    /* 1. Stack */
    ldr  sp, =_ulmk_kernel_stack_top

    /* 2. Context pool (arch-specific) */
    /* ... */

    /* 3+4. Vectors + ISR stack (arch-specific) */
    /* ... */

    /* 5. Board init */
    bl   ulmk_board_init

    /* 6. .data copy */
    /* ldr r0, =_ulmk_kernel_data_load; ldr r1, =_ulmk_kernel_data_start; ... */

    /* 7. .bss zero */
    /* ldr r0, =_ulmk_kernel_bss_start; ldr r1, =_ulmk_kernel_bss_end; ... */

    /* 8. Arch init */
    sub  sp, sp, #sizeof_ulmk_boot_info_t
    mov  r0, sp
    bl   ulmk_arch_init

    /* 9. Kernel main */
    bl   ulmk_kern_main
    /* ulmk_kern_main does not return */
_hang:
    b    _hang
```

The linker symbol names to use for `.data` and `.bss` boundaries are defined in
`linker/kernel/kernel_data.ld.in`:

```
_ulmk_kernel_data_load    LMA of .kernel_data (in flash)
_ulmk_kernel_data_start   VMA start (in RAM)
_ulmk_kernel_data_end     VMA end
_ulmk_kernel_bss_start
_ulmk_kernel_bss_end
```

---

## Step 4 — vectors.S

Provide handlers for:

1. **Trap/fault handler entry** — read fault info, call
   `ulmk_arch_trap_entry(class, tin)`.
2. **Syscall entry** — read syscall number and arguments, call
   `ulmk_arch_syscall_entry()` which calls `ulmk_kern_trap_syscall()`.
3. **Generic hardware ISR stub** — read interrupt number (SRPN), call
   `ulmk_kern_irq_dispatch(srpn)` then `ulmk_kern_sched_dispatch(true)`.

Each ISR stub must save the context required by the ABI and restore it before
returning.  On architectures with hardware-assisted context save (TriCore,
SPARC), leverage the hardware.  On software-save architectures (ARM Cortex-M),
push the caller-saved registers manually.

---

## Step 5 — arch.c (all ulmk_arch_* functions)

Implement every function declared in `ulmk_arch.h`.  Group them logically:

```c
/* ── CPU control ─────────────────── */
ulmk_arch_irq_key_t ulmk_arch_cpu_irq_save(void) { ... }
void ulmk_arch_cpu_irq_restore(ulmk_arch_irq_key_t key) { ... }
void ulmk_arch_cpu_irq_enable(void)  { ... }
void ulmk_arch_cpu_irq_disable(void) { ... }
void ulmk_arch_cpu_idle(void)  { ... }
void ulmk_arch_cpu_halt(void)  { for (;;) {} }
uint32_t ulmk_arch_cpu_clz(uint32_t v) { return __builtin_clz(v); }

/* ── Context pool ──────────────────── */
/* ulmk_arch_csa_pool_init (if applicable) */

/* ── Context init / switch ─────────── */
void ulmk_arch_ctx_init(ulmk_arch_ctx_t *ctx, ...) { ... }
void ulmk_arch_ctx_free(ulmk_arch_ctx_t *ctx)       { ... }
/* ulmk_arch_ctx_switch is in ctx_switch.S */

/* ── MPU ────────────────────────────── */
void ulmk_arch_mpu_init(void)      { ... }
void ulmk_arch_mpu_enable(void)    { ... }
void ulmk_arch_mpu_disable(void)   { ... }
void ulmk_arch_mpu_configure(...)  { ... }
void ulmk_arch_mpu_switch(...)     { ... }
bool ulmk_arch_mpu_addr_permitted(...) { ... }

/* ── IRQ / interrupt controller ─────── */
void ulmk_arch_irq_vectors_init(...)    { ... }
void ulmk_arch_irq_src_configure(...)   { ... }
void ulmk_arch_irq_src_enable(uint8_t srpn)  { ... }
void ulmk_arch_irq_src_disable(uint8_t srpn) { ... }
void ulmk_arch_irq_src_ack(uint8_t srpn)     { ... }
bool ulmk_arch_irq_src_is_pending(uint8_t srpn) { ... }
void ulmk_arch_irq_src_trigger(uint8_t srpn) { ... }

/* ── IRQ source map (for ulmk_irq_bind_hw) ── */
void ulmk_arch_irq_src_register(uint8_t srpn, uint32_t src_reg_addr) { ... }

/* ── Atomics ────────────────────────── */
uint32_t ulmk_arch_atomic_cas(...)  { ... }
uint32_t ulmk_arch_atomic_add(...) { ... }

/* ── Boot entry ─────────────────────── */
void ulmk_arch_init(ulmk_boot_info_t *info) { ... }
void ulmk_arch_trap_entry(uint8_t class, uint8_t tin) { ... }
void ulmk_arch_syscall_entry(void) { ... }
void ulmk_arch_trap_dump(uint8_t class, uint8_t tin) { ... }
```

---

## Step 6 — ctx_switch.S

Implement `ulmk_arch_ctx_switch(ulmk_arch_ctx_t *from, const ulmk_arch_ctx_t *to)`.

The kernel guarantees that interrupts are disabled when this is called.

**Software-save example (ARM Cortex-M, AAPCS):**

```asm
    .global ulmk_arch_ctx_switch
ulmk_arch_ctx_switch:          /* r0 = from, r1 = to */
    /* Save callee-saved registers on current stack */
    push  {r4-r11, lr}
    /* Save SP into from->sp */
    str   sp, [r0]
    /* Load SP from to->sp */
    ldr   sp, [r1]
    /* Restore callee-saved registers */
    pop   {r4-r11, pc}       /* pc = saved lr = return address */
```

**Hardware-save example (TriCore CSA):**

```asm
    svlcx                    /* save lower context */
    mfcr  d15, #PCXI
    st.w  [a4], d15          /* from->pcxi = PCXI */
    ld.w  d15, [a5]          /* d15 = to->pcxi */
    dsync
    mtcr  #PCXI, d15
    isync
    rslcx
    rfe
```

---

## Step 7 — Linker Fragments

### Mandatory: prologue.ld.in

```ld
/* arch/<name>/linker/prologue.ld.in */
OUTPUT_FORMAT("elf32-<name>")
OUTPUT_ARCH(<name>)
ENTRY(_start)
```

### Optional arch sections

Add extra `.ld.in` fragments for any arch-specific sections (register-window
spill area, hardware interrupt stack, etc.).  Register them in `generate_ld.py`
under the appropriate `HAVE_*` flag, following the pattern of
`arch/tricore/linker/csa_pool.ld.in`.

### memory.ld symbols required by the kernel

The chip's `memory.ld` must define (in addition to the MEMORY block):

```ld
ULMK_MPU_ALIGN         = 64;     /* byte alignment for MPU boundaries */
ULMK_KERNEL_STACK_SIZE = 4096;
ULMK_ISR_STACK_SIZE    = 2048;
```

TriCore-only extras: `ULMK_CSA_POOL_SIZE`, `HAVE_CSA`, `HAVE_SMALL_DATA`,
`HAVE_BMHD`.

---

## Step 8 — Toolchain CMake File

```cmake
# cmake/toolchain-<name>-<compiler>.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR <name>)

set(TRIPLE "<arch>-elf")
find_program(CMAKE_C_COMPILER   "${TRIPLE}-gcc"    REQUIRED)
find_program(CMAKE_ASM_COMPILER "${TRIPLE}-gcc"    REQUIRED)
find_program(CMAKE_OBJCOPY      "${TRIPLE}-objcopy" REQUIRED)
find_program(CMAKE_SIZE         "${TRIPLE}-size"    REQUIRED)

set(CMAKE_C_FLAGS_INIT   "-ffunction-sections -fdata-sections -ffreestanding -DULMK_KERNEL_BUILD")
set(CMAKE_ASM_FLAGS_INIT "")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

Add any arch-specific flags (e.g., `-mcpu=`, `-mthumb`, `-mfloat-abi=`) that
apply to all builds for this ISA.  Board-specific flags go in `board.cmake`.

---

## Step 9 — Board Directory

Even for QEMU bring-up, create a minimal board directory:

```
boards/<board>/
├── board.cmake
├── board_config.h    ← SoC MMIO bases, IRQ ctrl, platform quirks
├── memory.ld
├── board_console.c    ← ulmk_printk_char_out (semihosting, UART, or MMIO)
└── board_services.c   ← ulmk_board_init + board_services_init
```

`board_config.h` is on the compiler include path via `${ULMK_CHIP_DIR}` (CMake
and integration tests).  `arch/<isa>/arch_config.h` includes it and holds only
ISA invariants (CSFR addresses, CSA layout, standard IRQ-controller offsets).
Future boards may generate `board_config.h` from a DTS `reg` property.

See `boards/board_config.h.template` for required symbols per architecture.

---

## Step 10 — Wire CMakeLists.txt

Update the top-level `CMakeLists.txt` to accept the new toolchain:

```cmake
# Option A: document the command line (no change needed)
#   cmake -B build \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-<name>-gcc.cmake \
#       -DULMK_CHIP_DIR=boards/<board>

# Option B: make it the default for the arch
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_SOURCE_DIR}/cmake/toolchain-<name>-gcc.cmake"
        CACHE FILEPATH "" FORCE)
endif()
```

If the new arch has no hardware-managed context pool (i.e., no TriCore CSA),
set `HAVE_CSA = 0` in `memory.ld` and ensure `generate_ld.py` skips
`csa_pool.ld.in`.  Do the same for `HAVE_SMALL_DATA` and `HAVE_BMHD`.

---

## Step 11 — Validate with Integration Tests

The integration tests in `tests/` are standalone Makefiles that compile the
full kernel with their own source list.  To run them for the new arch:

1. Update each test Makefile's compile command to use the new toolchain and
   board sources.  Look for the pattern:

   ```makefile
   CC := tricore-elf-gcc
   ARCH_SRCS := \
       ../../arch/tricore/startup.S \
       ../../arch/tricore/arch.c \
       ../../arch/tricore/ctx_switch.S \
       ../../arch/tricore/vectors.S
   BOARD_SRCS := ../../boards/qemu_tc3xx/qemu_console.c
   ```

   Replace `tricore` with `<name>` and the board sources with your board.

2. Run each test and verify it prints `PASS`:

   ```bash
   cd tests/boot && make gen_config && make && make run
   cd tests/ctx_switch && make gen_config && make && make run
   # ... all 8 tests
   ```

3. All 8 tests must pass before the port is considered complete.

---

## 15. Common Pitfalls

### Context switch

- **`ulmk_arch_ctx_t` must be the first field of `ulmk_thread_t`.**  The assembly
  switch path loads the `from` pointer and writes at offset 0.  If it is not
  first, the saved state corrupts another TCB field.
- **Interrupts must be disabled** during `ulmk_arch_ctx_switch`.  The kernel
  ensures this, but the asm must not re-enable them.
- On software-save architectures, ensure the **full callee-save set** is
  pushed/popped.  A missing register will appear as a spurious corruption bug
  that surfaces only under certain call depth combinations.

### ISR stack

- The ISR stack must be initialised before enabling interrupts.
- Its size must be sufficient for the deepest nested ISR path.
- On architectures where interrupts automatically switch to a dedicated stack
  (TriCore `PSW.IS`, ARM `MSP`), verify the switch happens correctly on first
  IRQ entry.

### Preemption and IRQ dispatch

- Generic ISR stubs must call `ulmk_kern_irq_dispatch()` then
  `ulmk_kern_sched_dispatch(true)` **before** restoring context when a
  notification wakeup may have readied a higher-priority thread.
- There is no kernel tick timer.  Board services bind device IRQs (e.g. STM0
  compare-match) via `ulmk_irq_bind_hw()` and implement sleep/timekeeping in
  userspace.
- Timer clock constants (e.g. STM0 Hz) belong in board source, not kernel
  `config.cmake`.

### Linker script

- The `.startup` section must be the **first section in KERNEL_FLASH** so that
  the reset address lands on `_start`.  On QEMU targets, QEMU loads to the
  reset address, not the ELF entry point field.
- If the arch uses a small-data ABI, ensure the GP/SDA register is set in
  `startup.S` before any C function is called.

### Atomic operations

- Never use `__sync_*` builtins without verifying they compile to lock-free
  instructions on the target.  On single-core embedded systems a
  disable-IRQ + load + store + enable-IRQ sequence is acceptable, but it must
  truly be interrupt-safe.

### `ulmk_printk_char_out`

- Must be callable before the scheduler starts (before `ulmk_arch_init` returns).
  Do not depend on initialised globals or kernel objects.

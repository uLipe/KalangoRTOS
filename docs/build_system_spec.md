# ulipeMicroKernel — Build System Specification

**Version:** 1.0
**Status:** Reflects the implemented build system as of the component system introduction.

> **Purpose of this document:** authoritative reference for the CMake build model,
> directory layout, component scan rules, chip parameterisation, and kernel
> configuration symbols.  Consult this before changing `CMakeLists.txt`,
> `cmake/*.cmake`, or adding a new board or component.

---

## Table of Contents

1. [Design Philosophy](#1-design-philosophy)
2. [Binary Model](#2-binary-model)
3. [Directory Structure](#3-directory-structure)
4. [Component Discovery](#4-component-discovery)
5. [Component CMake API](#5-component-cmake-api)
6. [Build Flow](#6-build-flow)
7. [Chip Parameterisation](#7-chip-parameterisation)
8. [Board Descriptor](#8-board-descriptor)
9. [Root Thread and Stubs](#9-root-thread-and-stubs)
10. [Kernel Configuration](#10-kernel-configuration)
11. [dev.py — Container Frontend](#11-devpy--container-frontend)
12. [Bootloader Boundary](#12-bootloader-boundary)
13. [Phase 2 — CMake Package Export](#13-phase-2--cmake-package-export)

---

## 1. Design Philosophy

The build system follows the same layering principle as the linker and arch specs:
**each layer owns its concerns, nothing bleeds across boundaries**.

```
┌──────────────────────────────────────────────────────────────────────┐
│  ulipeMicroKernel/          Kernel repo — kernel source, arch port,  │
│                             linker fragments, CMake API, boards/,    │
│                             built-in components, and stub files.     │
├──────────────────────────────────────────────────────────────────────┤
│  ../ulmk_apps/              Optional sibling — external components   │
│                             and board-specific root threads.         │
├──────────────────────────────────────────────────────────────────────┤
│  <any path>/my_board/       Board chip input (external, optional) —  │
│                             memory.ld + bmhd.ld.in for real hw.      │
│                             Pointed to by UL_CHIP_DIR.               │
├──────────────────────────────────────────────────────────────────────┤
│  build/                     Generated artefacts — generated.ld,      │
│                             object files, final ELF. Never committed.│
└──────────────────────────────────────────────────────────────────────┘
```

**Principles:**

- Output is always **one ELF**.  Isolation is logical (MPU sections), not physical
  (separate ELF files).
- The kernel builds standalone.  If no component declares `ROOT_THREAD`, it links
  with a commented-out stub as documentation — see §9.
- No component name, domain name, or source path is hardcoded in the kernel.  All
  of that comes from component `CMakeLists.txt` files.
- The linker script is **generated at configure time** from fragments, after all
  components have registered themselves.
- Chip-specific inputs (memory map, boot header) are parameterised via `UL_CHIP_DIR`.

---

## 2. Binary Model

```
ulipe_microkernel  (ELF)
│
├── KERNEL_FLASH
│   ├── .startup           ← _start entry (must be first; QEMU starts at 0x80000000)
│   ├── .trap_table        ← trap vector table  (arch/tricore, ALIGN(256))
│   ├── .int_table         ← interrupt vector table (arch/tricore, ALIGN(256))
│   ├── .kernel_text       ← kernel supervisor code + rodata + component code
│   └── .domain_table      ← ul_domain_desc_t array (scanned at boot)
│
└── KERNEL_RAM
    ├── .kernel_data       ← kernel .data + .bss (supervisor only)
    ├── .kernel_stack      ← kernel supervisor stack
    ├── .isr_stack         ← ISR stack (TriCore ISP)
    ├── .csa_pool          ← TriCore CSA pool (ALIGN(64), NOLOAD)
    ├── .sdata / .sbss     ← TriCore small-data ABI
    └── .user_pool         ← physical allocator pool (remainder of KERNEL_RAM)
```

Component code currently lands in `.kernel_text` together with kernel code.
Per-component MPU domains are the mechanism for data isolation, not separate
code sections.

---

## 3. Directory Structure

```
ulipeMicroKernel/
├── CMakeLists.txt                        ← top-level orchestrator
├── cmake/
│   ├── toolchain-tricore-gcc.cmake       ← toolchain (-mcpu, sysroot, …)
│   ├── config.cmake                      ← 6 kernel config symbols (cache vars)
│   ├── config.h.in                       ← template → build/include/ul/config.h
│   ├── component_api.cmake               ← ul_component_register(), ul_components_finalize()
│   ├── linker_api.cmake                  ← ul_add_app(), ul_add_domain(), ul_generate_linker_script()
│   └── generate_ld.py                    ← assembles build/generated/ulipe_microkernel.ld
├── kernel/                               ← kernel implementation (sched, ipc, mem, irq, …)
├── arch/
│   └── tricore/
│       ├── arch.c  ctx_switch.S  vectors.S  startup.S
│       ├── include/
│       │   ├── arch_config.h             ← TriCore constants (UL_ARCH_NUM_DPR, etc.)
│       │   └── ul_arch.h                 ← arch contract header
│       └── linker/
│           ├── prologue.ld.in            ← OUTPUT_FORMAT, OUTPUT_ARCH, ENTRY
│           ├── csa_pool.ld.in            ← CSA pool fragment
│           └── small_data.ld.in          ← small-data ABI anchors
├── linker/
│   └── kernel/                           ← 6 arch-independent kernel fragments
│       ├── vectors.ld.in                 ← .startup + .trap_table + .int_table
│       ├── kernel_text.ld.in
│       ├── kernel_data.ld.in
│       ├── kernel_stacks.ld.in
│       ├── domain_table.ld.in
│       └── user_pool.ld.in
├── include/
│   └── ul/
│       ├── microkernel.h                 ← public kernel API (all syscall wrappers)
│       ├── linker.h                      ← UL_DOMAIN_BSS, UL_PRIVATE, UL_DEFINE_DOMAIN
│       ├── syscall_nr.h                  ← syscall number table
│       └── syscall_abi.h                 ← UL_SYSCALL_N macros
├── components/                           ← built-in components (hello_world, …)
│   └── hello_world/
│       ├── CMakeLists.txt
│       ├── include/hello_world.h
│       └── src/root_thread.c  hello_world.c
├── boards/
│   └── qemu_tc3xx/                       ← ONLY built-in board (CI / QEMU)
│       ├── board.cmake                   ← sets UL_BOARD_CPU, UL_BOARD_CFLAGS, UL_BOARD_SOURCES
│       ├── memory.ld                     ← MEMORY block + linker flags (no BMHD for QEMU)
│       ├── qemu_console.c                ← ul_printk_char_out() via MMIO
│       ├── board_console.c / .h          ← IPC-backed console service
│       └── board_services.c / .h         ← ul_board_init() + board_services_init()
├── stub/
│   ├── board_init_stub.c                 ← non-weak no-op ul_board_init(); used by test Makefiles
│   ├── board_services_stub.c             ← DOCUMENTATION ONLY (commented out)
│   ├── printk_stub.c                     ← DOCUMENTATION ONLY (commented out)
│   └── root_thread_stub.c                ← DOCUMENTATION ONLY (commented out)
├── tests/                                ← integration tests (standalone Makefiles)
└── tools/
    ├── dev.py                            ← container frontend (cmake configure + build)
    └── docker/
```

External layout:

```
ulipeMicroKernel/            ← kernel repo (this)
../ulmk_apps/                ← optional sibling; auto-discovered if present

<anywhere>/my_board/         ← real board chip input (external)
    memory.ld                ← MEMORY block with real HW addresses
    bmhd.ld.in               ← chip boot header (if HAVE_BMHD = 1)
```

---

## 4. Component Discovery

### 4.1 Scan locations

The top-level `CMakeLists.txt` scans two locations at configure time:

1. `kernel/components/` — built-in components shipped with the kernel repo.
2. `../ulmk_apps/` — optional external components sibling directory.

For each directory found, if it contains a `CMakeLists.txt`, it is added via
`add_subdirectory()`.  That `CMakeLists.txt` must call `ul_component_register()`
exactly once.

```cmake
# From CMakeLists.txt — simplified
file(GLOB _dirs LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/components/*")
foreach(_dir IN LISTS _dirs)
    if(IS_DIRECTORY "${_dir}" AND EXISTS "${_dir}/CMakeLists.txt")
        add_subdirectory("${_dir}" ...)
    endif()
endforeach()
# Same pattern for ../ulmk_apps/
```

### 4.2 Discovery log

The configure step prints a discovery log:

```
-- Scanning components:
--   [component] hello_world ENABLED
--   [component] ROOT_THREAD: hello_world
```

### 4.3 Validation

`ul_components_finalize()` (called after all scans) validates:

- At most one component declares `ROOT_THREAD`.  If more than one does, the
  build fails with a clear error.
- If a component's `REQUIRES` dependency is registered as `DISABLED`, the
  build fails immediately.

If no component declares `ROOT_THREAD`, a warning is printed and the build
continues — the link will fail with an undefined reference to `ul_root_thread`,
which is the intended diagnostic.

---

## 5. Component CMake API

Defined in `cmake/component_api.cmake`, included automatically by the top-level.

### `ul_component_register`

```cmake
ul_component_register(
    NAME         <name>           # unique identifier
    ENABLED      <ON|OFF>         # whether to include this component
    SOURCES      <files…>         # source files (paths relative to component dir)
    INCLUDE_DIRS <dirs…>          # public include dirs added globally
    REQUIRES     <components…>    # (optional) dependency names
    LINKER_FRAGMENT <file>        # (optional) .ld.in appended to generated script
    ROOT_THREAD                   # (flag) this component provides ul_root_thread()
)
```

When `ENABLED` is `ON`:

- Sources are added to the `ulipe_kernel` static library target.
- Include directories are added as `PUBLIC` so all other targets see them.
- If `ROOT_THREAD` is set, the component name is recorded; `ul_components_finalize()`
  errors if more than one component sets this flag.

When `ENABLED` is `OFF`, the component is skipped entirely and emits a
`DISABLED` log line.

### `ul_components_finalize`

```cmake
ul_components_finalize()
```

Called once after all component scans.  Validates ROOT_THREAD invariant and
prints the summary line.

---

## 6. Build Flow

```
cmake -B /build/ulipe \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-tricore-gcc.cmake \
      -DUL_CHIP_DIR=boards/qemu_tc3xx
│
├─ cmake/config.cmake          → 6 config symbols (cache vars)
│   configure_file(config.h.in → build/include/ul/config.h)
│
├─ cmake/component_api.cmake   → defines ul_component_register, ul_components_finalize
│
├─ ${UL_CHIP_DIR}/board.cmake  → sets UL_BOARD_CPU, UL_BOARD_CFLAGS, UL_BOARD_SOURCES
│
├─ add_library(ulipe_kernel STATIC
│       kernel/**/*.c
│       arch/tricore/arch.c  ctx_switch.S
│       ${UL_BOARD_SOURCES})
│
├─ Component scan:
│   for each dir in components/ and ../ulmk_apps/:
│       add_subdirectory → ul_component_register()
│         sources → target_sources(ulipe_kernel …)
│         includes → target_include_directories(ulipe_kernel PUBLIC …)
│
├─ ul_components_finalize()    → validate ROOT_THREAD invariant
│
├─ cmake/linker_api.cmake      → ul_generate_linker_script()
│   PRE_LINK: generate_ld.py → build/generated/ulipe_microkernel.ld
│
└─ add_executable(ulipe_microkernel
       arch/tricore/startup.S
       arch/tricore/vectors.S)
   target_link_libraries(ulipe_microkernel PRIVATE ulipe_kernel)
   target_link_options(-T build/generated/ulipe_microkernel.ld)

cmake --build /build/ulipe
│
├─ compile all sources in ulipe_kernel (kernel + board + components)
├─ compile startup.S + vectors.S
├─ [PRE_LINK] generate_ld.py → build/generated/ulipe_microkernel.ld
└─ link → build/ulipe/ulipe_microkernel  (ELF)

Boot sequence (inside the ELF):
  _start (startup.S)
    ├─ ul_board_init()          ← provided by board; no globals, no kernel API
    ├─ .data copy (LMA→VMA) + .bss zero
    ├─ ul_arch_init()           ← fills ul_boot_info_t
    └─ ul_kernel_main()         ← does not return; starts scheduler
          └─ ul_root_thread()   ← first userspace context
```

---

## 7. Chip Parameterisation

```bash
cmake -B build -DUL_CHIP_DIR=boards/qemu_tc3xx          # QEMU (default)
cmake -B build -DUL_CHIP_DIR=/path/to/my_board           # real hardware
```

`UL_CHIP_DIR` must point to a directory containing:

| File | Required | Description |
|------|----------|-------------|
| `memory.ld` | yes | `MEMORY {}` block + linker flags (`UL_MPU_ALIGN`, `HAVE_CSA`, etc.) |
| `bmhd.ld.in` | only if `HAVE_BMHD=1` | Chip boot header section |
| `board.cmake` | yes | Sets `UL_BOARD_CPU`, `UL_BOARD_CFLAGS`, `UL_BOARD_SOURCES` |

Full chip input contract: `docs/linker_spec.md §9`.

---

## 8. Board Descriptor

Each board directory must contain a `board.cmake` with:

```cmake
set(UL_BOARD_CPU   "tc39xx")           # passed to -mcpu=
set(UL_BOARD_CFLAGS "-DUL_ARCH_QEMU_VIRT_CONSOLE=1 …")
set(UL_BOARD_SOURCES
    qemu_console.c
    board_console.c
    board_services.c
)
```

`UL_BOARD_SOURCES` paths are relative to `${UL_CHIP_DIR}`.  They are added to
`ulipe_kernel` and must provide at minimum:

| Symbol | Where | When called |
|--------|-------|-------------|
| `ul_board_init(void)` | `board_services.c` | From `startup.S` before `.data` copy |
| `ul_printk_char_out(char)` | `qemu_console.c` or equivalent | By kernel printk subsystem |
| `board_services_init(const ul_boot_info_t *)` | `board_services.c` | From `ul_root_thread()` |

---

## 9. Root Thread and Stubs

### 9.1 Contract

Exactly one component must declare `ROOT_THREAD` and provide:

```c
#include <ul/microkernel.h>

void ul_root_thread(const ul_boot_info_t *info)
{
    board_services_init(info);  /* start board services */
    my_component_init();        /* start app components */
    ul_thread_exit();
}
```

### 9.2 Stubs (documentation only)

The files in `stub/` (except `board_init_stub.c`) are **not compiled**.  They
exist solely as documentation showing what each symbol must look like:

| File | Symbol documented |
|------|------------------|
| `stub/root_thread_stub.c` | `ul_root_thread()` |
| `stub/printk_stub.c` | `ul_printk_char_out()` |
| `stub/board_services_stub.c` | `board_services_init()` |

`stub/board_init_stub.c` **is compiled** — only by integration test Makefiles
that include it directly.  It provides a non-weak no-op `ul_board_init()` for
QEMU test builds.  The CMake build never includes it; the board provides
`ul_board_init()` from its own sources.

### 9.3 Why no weak symbols

Weak symbols in a static library (`libulipe_kernel.a`) are resolved by the
first archive object that satisfies the reference, preventing the strong
definition in a later archive object from overriding it.  Using weak symbols
caused the test stubs to silently override the real board/component
implementations.  All board- and component-provided symbols are now **strong**,
and a missing symbol produces a link-time error rather than a silent no-op.

---

## 10. Kernel Configuration

### 10.1 Symbols

Defined in `cmake/config.cmake`:

```
┌──────────────────────────────────┬──────────┬────────────────────────────────────┐
│ Symbol                           │ Default  │ What it controls                   │
├──────────────────────────────────┼──────────┼────────────────────────────────────┤
│ UL_CONFIG_MAX_THREADS            │ 32       │ Static TCB pool                    │
│ UL_CONFIG_MAX_ENDPOINTS          │ 64       │ Static IPC endpoint pool           │
│ UL_CONFIG_MAX_NOTIFS             │ 32       │ Static notification pool           │
│ UL_CONFIG_MAX_IRQ_BINDINGS       │ 16       │ SRPN → notif binding table         │
│ UL_CONFIG_TICK_HZ                │ 1000     │ Scheduler tick rate (Hz)           │
│ UL_CONFIG_HW_SYS_CLOCK_HZ       │ 50000000 │ System clock fed to tick timer     │
└──────────────────────────────────┴──────────┴────────────────────────────────────┘
```

`UL_CONFIG_HW_SYS_CLOCK_HZ` defaults to 50 MHz — the QEMU TC397B STM0 rate.
**Override per board** on real hardware.

### 10.2 Override

```bash
cmake -B build \
    -DUL_CHIP_DIR=boards/qemu_tc3xx \
    -DUL_CONFIG_MAX_THREADS=16 \
    -DUL_CONFIG_HW_SYS_CLOCK_HZ=200000000
```

### 10.3 Generated header

`configure_file(cmake/config.h.in ${CMAKE_BINARY_DIR}/include/ul/config.h)`

The header is **kernel-internal only**.  User code must not include it.

### 10.4 Scheduler quantum

Two derived symbols are required by the scheduler; they are computed and written
into `config.h` by `config.cmake`:

```cmake
# Scheduler quantum
set(UL_CONFIG_SCHED_QUANTUM_US    10000)   # 10 ms
set(UL_CONFIG_SCHED_QUANTUM_TICKS ...)     # = QUANTUM_US / (1000000 / TICK_HZ)
```

Forgetting `UL_CONFIG_SCHED_QUANTUM_TICKS` causes every tick to trigger
preemption — this is the most common cause of scheduler test failures.

---

## 11. dev.py — Container Frontend

`tools/dev.py` is a thin CMake frontend that runs inside the dev container.

```bash
python3 tools/dev.py              # enter container (builds image on first run)
python3 tools/dev.py --rebuild    # force image rebuild

# Inside the container:
python3 tools/dev.py build                      # configure + build (QEMU board)
python3 tools/dev.py build --clean              # rm -rf build + configure + build
python3 tools/dev.py build --board /path/board  # custom UL_CHIP_DIR
python3 tools/dev.py run                        # build (if stale) + run on QEMU
```

`dev.py` does not maintain a parallel source list.  CMake is the single source
of truth for what gets compiled.

---

## 12. Bootloader Boundary

```
WITHOUT external bootloader (common case):
  power-on / reset vector
       └─ _start → ul_board_init() → .data/.bss → ul_arch_init() → ul_kernel_main()

WITH external bootloader (when needed):
  ROM / Stage-0 (PLL, clocks already done)
       └─ jump to _start → ul_board_init() [no-op] → … → ul_kernel_main()
```

For QEMU: `-kernel` loads to `0x80000000` and starts there, not at the ELF
entry point.  The `.startup` linker section in `linker/kernel/vectors.ld.in`
ensures `_start` is placed at `0x80000000`.

---

## 13. Phase 2 — CMake Package Export

Future: the kernel will be installable as a CMake package:

```bash
cmake --install build --prefix /opt/ulipeMicroKernel
```

An external app:

```cmake
find_package(UlipeMicroKernel REQUIRED HINTS /opt/ulipeMicroKernel)
ul_component_register(NAME my_app SOURCES my_app.c INCLUDE_DIRS include ROOT_THREAD ENABLED ON)
ul_components_finalize()
ul_generate_linker_script()
```

Phase 2 requires only CMake packaging infrastructure — no kernel source changes.

---

*Implementation lives in `cmake/`, `boards/`, `stub/`, `components/`, and the
top-level `CMakeLists.txt`.  Generated artefacts are in `build/`.*

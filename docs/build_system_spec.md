# ulipeMicroKernel — Build System Specification

**Version:** 0.1 (draft)

---

## Table of Contents

1. [Design Philosophy](#1-design-philosophy)
2. [Binary Model](#2-binary-model)
3. [Directory Structure](#3-directory-structure)
4. [App Discovery — Sibling Directory Model](#4-app-discovery--sibling-directory-model)
5. [CMake API](#5-cmake-api)
6. [Build Flow](#6-build-flow)
7. [Chip Parameterisation](#7-chip-parameterisation)
8. [Root Thread and Stub](#8-root-thread-and-stub)
9. [Kernel Configuration](#9-kernel-configuration)
10. [Phase 2 — CMake Package Export](#10-phase-2--cmake-package-export)
11. [Bootloader Boundary](#11-bootloader-boundary)
12. [Implementation Roadmap](#12-implementation-roadmap)

---

## 1. Design Philosophy

The build system follows the same layering principle as the linker and arch specs:
**each layer owns its concerns, nothing bleeds across boundaries**.

```
┌──────────────────────────────────────────────────────────────────────┐
│  ulipeMicroKernel/          Kernel repo — kernel source, arch port,  │
│                             linker fragments, CMake API, stub files,  │
│                             and the built-in QEMU board only.         │
├──────────────────────────────────────────────────────────────────────┤
│  ulipeMicroKernel_apps/     Apps repo (sibling, optional) — drivers, │
│                             application tasks, ul_root_thread(), and  │
│                             optionally a ul_board_init() for the      │
│                             target board.                             │
├──────────────────────────────────────────────────────────────────────┤
│  <any path>/my_board/       Board chip input (external, optional) —  │
│                             memory.ld + bmhd.ld.in for real hardware. │
│                             Pointed to by UL_CHIP_DIR.               │
├──────────────────────────────────────────────────────────────────────┤
│  build/                     Generated artefacts — generated.ld,      │
│                             object files, final ELF. Never committed. │
└──────────────────────────────────────────────────────────────────────┘
```

**Principles:**

- The output is always **one ELF**. Isolation is logical (MPU sections), not physical
  (separate ELF files). A second-stage bootloader may be added later without changing
  this model.
- The kernel builds standalone. If no apps repo is present, it links with a stub
  `ul_root_thread()` that immediately exits. This allows CI to validate the kernel
  independently.
- No app name, domain name, or source path is hardcoded in the kernel. All of that
  comes from the apps repo through the CMake API.
- The linker script is **generated at configure time** from fragments, after all apps
  have registered themselves via `ul_add_app()` and `ul_add_domain()`.
- Chip-specific inputs (memory map, chip boot header) are parameterised via
  `UL_CHIP_DIR`. The kernel never contains concrete chip addresses.

---

## 2. Binary Model

The single output ELF contains all components in their respective linker sections:

```
ulipe.elf
│
├── KERNEL_FLASH
│   ├── .bmhd              ← chip boot header (chip input, optional)
│   ├── .trap_table        ← trap vector table  (arch/tricore)
│   ├── .int_table         ← interrupt vector table (arch/tricore)
│   ├── .kernel_text       ← kernel supervisor code + rodata
│   ├── .app_NAME_text     ← one section per app (MPU-aligned, ALIGN(UL_MPU_ALIGN))
│   └── .domain_table      ← ul_domain_desc_t array (scanned at boot)
│
└── KERNEL_RAM
    ├── .kernel_data       ← kernel .data + .bss (supervisor only)
    ├── .kernel_stack      ← kernel supervisor stack
    ├── .isr_stack         ← ISR stack (TriCore ISP)
    ├── .csa_pool          ← TriCore CSA pool (ALIGN(64), NOLOAD)
    ├── .domain_NAME       ← one section per domain (MPU-aligned, NOLOAD)
    ├── .sdata / .sbss     ← TriCore small-data ABI
    └── .user_pool         ← physical allocator pool (remainder of KERNEL_RAM)
```

"Monolithic" means a single link step. It does not mean shared memory — the MPU
enforces isolation between domains. The `kernel_main()` equivalent from the
reference book (hardcoded app names) is replaced by:

1. `UL_DEFINE_DOMAIN` macros populating `.domain_table` in flash.
2. `ul_root_thread()` user-provided function creating threads at boot.
3. The kernel scanning `.domain_table` at boot to register MPU ranges.

No app name appears in kernel source code.

---

## 3. Directory Structure

```
ulipeMicroKernel/                        ← this repo
├── CMakeLists.txt                        ← top-level orchestrator
├── cmake/
│   ├── toolchain-tricore-gcc.cmake       ← toolchain file (-mcpu, sysroot, …)
│   ├── linker_api.cmake                  ← ul_add_app(), ul_add_domain(),
│   │                                        ul_set_root_thread()
│   ├── generate_ld.py                    ← assembles build/generated.ld
│   └── UlipeMicroKernelConfig.cmake.in   ← Phase 2: package export template
├── kernel/                               ← kernel implementation
│   ├── sched/
│   ├── ipc/
│   ├── mem/
│   └── irq/
├── arch/
│   └── tricore/
│       ├── startup.S
│       ├── vectors.S
│       └── linker/
│           ├── prologue.ld.in            ← OUTPUT_FORMAT, OUTPUT_ARCH, ENTRY
│           ├── csa_pool.ld.in            ← CSA pool fragment
│           └── small_data.ld.in          ← small-data ABI anchors
├── linker/
│   ├── kernel/                           ← 6 arch-independent kernel fragments
│   │   ├── vectors.ld.in
│   │   ├── kernel_text.ld.in
│   │   ├── kernel_data.ld.in
│   │   ├── kernel_stacks.ld.in
│   │   ├── domain_table.ld.in
│   │   └── user_pool.ld.in
│   └── snippets/
│       ├── app_code.ld.in                ← template: per-app code section
│       └── domain_data.ld.in             ← template: per-domain data section
├── include/
│   └── ul/
│       ├── microkernel.h                 ← public kernel API
│       └── linker.h                      ← UL_DOMAIN_BSS, UL_PRIVATE, …
├── stub/
│   ├── root_thread_stub.c               ← weak ul_root_thread(): called when no apps repo
│   └── board_init_stub.c                ← weak ul_board_init(): no-op; overridden by user
├── boards/
│   └── qemu_tc27x/                      ← ONLY built-in board: needed for CI on QEMU
│       └── memory.ld                    ← MEMORY block + flags (no BMHD for QEMU)
├── tests/
└── tools/
    ├── dev.py
    ├── docker/
    └── hello/                           ← standalone hello-world (not part of main build)
```

The repos that sit beside the kernel:

```
ulipeMicroKernel/            ← kernel repo (this); ships only qemu_tc27x board
ulipeMicroKernel_apps/       ← apps repo (sibling, optional)
    CMakeLists.txt
    root_thread.c
    board_init.c             ← optional ul_board_init() for target chip
    drivers/
        asclin/
            CMakeLists.txt
            asclin.c
    apps/
        sensor/
            CMakeLists.txt
            sensor.c

<anywhere>/my_board/         ← real board chip input (external, not in kernel)
    memory.ld                ← MEMORY block with real HW addresses
    bmhd.ld.in               ← chip boot header (chip-family specific)
```

Real board chip inputs are **not** kept in the kernel repo. The kernel repo only
ships the QEMU board (`boards/qemu_tc27x/`) because it is required for built-in
CI via the Docker toolchain. Any real hardware board lives in the apps repo, in a
separate board repo, or anywhere on the filesystem — pointed to by `UL_CHIP_DIR`.

---

## 4. App Discovery — Sibling Directory Model

### 4.1 Phase 1 mechanism

The top-level `CMakeLists.txt` checks for the sibling at configure time:

```cmake
# Path can be overridden for non-standard layouts
set(UL_APPS_DIR "${CMAKE_SOURCE_DIR}/../ulipeMicroKernel_apps"
    CACHE PATH "Path to the apps repository (leave empty for kernel-only build)")

if(EXISTS "${UL_APPS_DIR}/CMakeLists.txt")
    message(STATUS "[ul] apps found: ${UL_APPS_DIR}")
    add_subdirectory("${UL_APPS_DIR}" _apps)
    set(UL_HAS_APPS TRUE)
else()
    message(STATUS "[ul] no apps directory — building with stub root_thread")
    set(UL_HAS_APPS FALSE)
endif()
```

`UL_APPS_DIR` is a CMake cache variable, so it can be overridden from the command line:

```bash
# Standard layout — sibling auto-detected
cmake -B build -DUL_CHIP_DIR=boards/qemu_tc27x

# Non-standard layout
cmake -B build \
    -DUL_CHIP_DIR=boards/qemu_tc27x \
    -DUL_APPS_DIR=/path/to/my_apps
```

### 4.2 What the apps CMakeLists.txt does

The apps repo's `CMakeLists.txt` is **not a top-level project**. It is included as
a CMake subdirectory by the kernel's top-level. It may not call `cmake_minimum_required`
or `project()`. It calls only the `ul_*` API functions defined by `linker_api.cmake`.

```cmake
# ulipeMicroKernel_apps/CMakeLists.txt

ul_set_root_thread(SOURCE root_thread.c)

add_subdirectory(drivers/asclin)
add_subdirectory(apps/sensor)
```

```cmake
# ulipeMicroKernel_apps/drivers/asclin/CMakeLists.txt

ul_add_app(
    NAME    asclin_driver
    SOURCES asclin.c
    DOMAIN  asclin
    STACK   2048
    PRIV    DRIVER
)
ul_add_domain(
    NAME   asclin
    PERMS  "UL_PERM_READ | UL_PERM_WRITE | UL_PERM_USER"
    SHARED FALSE
)
```

### 4.3 Kernel-only build

When `UL_APPS_DIR` is absent or the directory does not exist, the kernel builds
with `stub/root_thread_stub.c`. The ELF is fully functional: the kernel
boots, initialises all subsystems, launches `ul_root_thread()`, which immediately
calls `ul_thread_exit()`. This is the baseline for kernel CI.

---

## 5. CMake API

Defined in `cmake/linker_api.cmake`. Included automatically by the top-level
`CMakeLists.txt` before `add_subdirectory(_apps)`.

### 5.1 `ul_add_app`

```cmake
ul_add_app(
    NAME    <name>      # unique identifier; becomes section prefix .app_<name>_text
    SOURCES <files...>  # source files compiled into this app's code section
    DOMAIN  <domain>    # associated data domain (optional)
    STACK   <bytes>     # thread stack size allocated from user_pool at boot
    PRIV    DRIVER|USER # privilege level (UL_PRIV_DRIVER or UL_PRIV_USER)
)
```

Internally:
- Appends `<name>` to `UL_APP_LIST` (used by `generate_ld.py`).
- Compiles `SOURCES` with `-DUL_APP_NAME=<name>` so functions land in
  `.text.<name>.<func>` sections, captured by the `app_code.ld.in` snippet.
- Stores STACK and PRIV in target properties for use by the kernel's thread
  creation table (a generated C file describing initial threads).

### 5.2 `ul_add_domain`

```cmake
ul_add_domain(
    NAME   <name>          # domain identifier; matches UL_MODULE_NAME in C code
    PERMS  "<expr>"        # UL_PERM_* expression string, written into generated header
    SHARED <TRUE|FALSE>    # TRUE → section placed in SHARED_RAM (multi-core visible)
)
```

Internally:
- Appends `<name>` to `UL_DOMAIN_LIST` (used by `generate_ld.py`).
- Records SHARED flag per domain.

### 5.3 `ul_set_root_thread`

```cmake
ul_set_root_thread(SOURCE <file>)
```

Registers the file that provides `void ul_root_thread(const ul_boot_info_t *info)`.
Only one call is allowed per build. If not called (kernel-only build), the stub
is used automatically.

### 5.4 Generate step

After all `ul_add_app()` / `ul_add_domain()` calls, the top-level `CMakeLists.txt`
calls:

```cmake
ul_generate_linker_script()
```

This adds a `PRE_LINK` custom command that runs `generate_ld.py`:

```
python3 cmake/generate_ld.py
    --chip-dir   ${UL_CHIP_DIR}
    --arch-dir   arch/tricore/linker
    --kernel-dir linker/kernel
    --snippets   linker/snippets
    --app-list   ${UL_APP_LIST}
    --dom-list   ${UL_DOMAIN_LIST}
    --output     ${CMAKE_BINARY_DIR}/generated.ld
```

The final ELF target then uses:

```cmake
target_link_options(ulipe.elf PRIVATE -T ${CMAKE_BINARY_DIR}/generated.ld)
```

---

## 6. Build Flow

```
cmake -B build -DUL_CHIP_DIR=boards/qemu_tc27x
│
├─ cmake/linker_api.cmake loaded
├─ sibling directory check:
│   ├─ found  → add_subdirectory(ulipeMicroKernel_apps)
│   │            ul_add_app(), ul_add_domain(), ul_set_root_thread() called
│   │            board_init.c (if present) compiled alongside app sources
│   └─ absent → stub/root_thread_stub.c registered
│                stub/board_init_stub.c always linked (weak; overridden if
│                user provides a strong ul_board_init() symbol)
│
├─ ul_generate_linker_script():
│   registers PRE_LINK command:
│     generate_ld.py → build/generated.ld
│
└─ add_executable(ulipe.elf
       kernel/**/*.c
       arch/tricore/startup.S
       arch/tricore/vectors.S
       stub/board_init_stub.c          ← weak ul_board_init(); user overrides
       <app sources registered by ul_add_app>
       <root_thread.c or stub>)
   target_link_options(-T build/generated.ld)
   target_include_directories(include/)

cmake --build build
│
├─ compile kernel, arch, stub sources
├─ compile app sources (each with -DUL_APP_NAME=<name>)
├─ compile root_thread (user or stub)
├─ [PRE_LINK] generate_ld.py → build/generated.ld
└─ link → build/ulipe.elf

Runtime boot sequence (inside the ELF):
  _start (startup.S)
    │  stack, ISP, BTV/BIV, small-data anchors, CSA pool
    ├─ ul_board_init()      ← user optional; weak no-op if absent
    │    PLL, flash WS, ext RAM — NO kernel API, NO .data vars
    ├─ [asm] .data copy (LMA→VMA) + .bss zero
    ├─ ul_arch_init()       ← arch-provided; fills ul_boot_info_t
    └─ ul_kernel_main()     ← kernel-internal; does not return
```

---

## 7. Chip Parameterisation

The chip is selected via the `UL_CHIP_DIR` CMake cache variable:

```bash
cmake -B build -DUL_CHIP_DIR=boards/qemu_tc27x   # QEMU
cmake -B build -DUL_CHIP_DIR=boards/tc27x         # real hardware
cmake -B build -DUL_CHIP_DIR=/external/my_board   # out-of-tree chip input
```

`UL_CHIP_DIR` must point to a directory containing at minimum a `memory.ld` file
satisfying the chip input contract (§3.1 of `docs/linker_spec.md`).

The toolchain file (`cmake/toolchain-tricore-gcc.cmake`) is selected separately:

```bash
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-tricore-gcc.cmake \
    -DUL_CHIP_DIR=boards/qemu_tc27x
```

For convenience, the top-level `CMakeLists.txt` sets the toolchain file as the
default when building for TriCore.

---

## 8. Root Thread, Board Init, and Stubs

### 8.0 ul_board_init — optional chip setup callback

`ul_board_init()` runs as the first C function in the system, before `.data` is
copied from flash and before any kernel subsystem is initialised.

```c
/* arch/tricore/board_init_stub.c — weak no-op; user overrides with strong symbol */
__attribute__((weak)) void ul_board_init(void) {}
```

The user provides a strong `ul_board_init()` anywhere in the link — in the apps
repo, in a board-specific file, or anywhere else. No CMake registration is needed;
the linker resolves the strong symbol over the weak stub automatically.

```c
/* ulipeMicroKernel_apps/board_init.c — example for TC27x */
void ul_board_init(void)
{
    /* Configure PLL: 20 MHz crystal → 200 MHz CPU clock */
    /* Set flash wait states for 200 MHz operation        */
    /* Enable external RAM if present                     */
}
```

**Constraints for ul_board_init:**
- Full supervisor privilege; MPU not yet active.
- No initialised global variables available (`.data` not yet copied).
- No `ul_*` kernel or arch API calls.
- No interrupt enable.
- Must return normally.

When a dedicated bootloader is used, `ul_board_init()` should remain the no-op
stub — the bootloader will have already configured hardware before jumping to
`_start`.

### 8.1 Contract

The apps repo provides exactly one file implementing:

```c
#include <ul/microkernel.h>

void ul_root_thread(const ul_boot_info_t *info)
{
    /* Spawn all initial threads, then exit or block. */
    ul_thread_exit();
}
```

This function is the first userspace context. It runs at `UL_PRIV_DRIVER`
privilege and holds the spawn capability. It must never return; it must call
`ul_thread_exit()` when bootstrapping is complete.

### 8.2 Stub

`stub/root_thread_stub.c` is compiled when no apps repo is present:

```c
#include <ul/microkernel.h>

void ul_root_thread(const ul_boot_info_t *info)
{
    (void)info;
    ul_thread_exit();
}
```

The stub produces a valid binary that boots the kernel completely, verifying all
internal initialisation (physical allocator, scheduler, MPU setup) without
requiring any application code. It is the foundation for kernel CI.

### 8.3 Generated thread table

`ul_add_app()` accumulates STACK and PRIV metadata. At configure time,
`generate_ld.py` (or a companion `generate_thread_table.py`) emits a C file:

```c
/* build/ul_thread_table.c — generated, do not edit */
#include <ul/microkernel.h>

const ul_thread_init_t _ul_thread_table[] = {
    { "asclin_driver", asclin_driver_entry, 2048, UL_PRIV_DRIVER,
      _ul_app_asclin_driver_text_start, _ul_app_asclin_driver_text_end,
      &__ul_domain_desc_asclin },
    { "sensor",        sensor_entry,        4096, UL_PRIV_USER,
      _ul_app_sensor_text_start,        _ul_app_sensor_text_end,
      &__ul_domain_desc_sensor  },
};
const size_t _ul_thread_table_count =
    sizeof(_ul_thread_table) / sizeof(_ul_thread_table[0]);
```

The kernel uses this table internally during boot to pre-create threads before
the scheduler starts. User code does not see this table; it interacts only via
`ul_root_thread()` and `ul_thread_create()`.

---

## 9. Kernel Configuration

### 9.1 Rationale

The kernel's configurable surface is intentionally tiny. Because policy lives in
userspace, the kernel itself has no subsystems to enable or disable, no protocol
stacks, no driver frameworks. After scanning all specification documents, only
**five symbols** require user tuning — all of them static resource limits or a
timing parameter. A Kconfig system or a separate configuration file would add
complexity without benefit. The configuration is handled entirely via CMake cache
variables and a single generated header.

### 9.2 The five configuration symbols

```
┌─────────────────────────────┬─────────┬────────────────────────────────────────┐
│ Symbol                      │ Default │ What it sizes                          │
├─────────────────────────────┼─────────┼────────────────────────────────────────┤
│ UL_CONFIG_MAX_THREADS       │   32    │ Static TCB pool in .kernel_data        │
│ UL_CONFIG_MAX_ENDPOINTS     │   64    │ Static EP object pool                  │
│ UL_CONFIG_MAX_NOTIFS        │   32    │ Static notification object pool        │
│ UL_CONFIG_MAX_IRQ_BINDINGS  │   16    │ SRPN → notif binding table             │
│ UL_CONFIG_TICK_HZ           │  1000   │ Scheduler tick rate; reported in       │
│                             │         │ ul_boot_info_t.tick_hz; sets timer     │
│                             │         │ reload value in ul_arch_tick_init()    │
└─────────────────────────────┴─────────┴────────────────────────────────────────┘
```

Everything else is either:
- A hardware constant → defined in `arch/<ARCH>/include/arch_config.h`
- A memory-sizing parameter → defined in the chip's `memory.ld`
- A policy decision → delegated to userspace

### 9.3 CMake interface

Defined in `cmake/config.cmake`, included by the top-level `CMakeLists.txt`:

```cmake
set(UL_CONFIG_MAX_THREADS      32   CACHE STRING "Max simultaneous threads")
set(UL_CONFIG_MAX_ENDPOINTS    64   CACHE STRING "Max IPC endpoints")
set(UL_CONFIG_MAX_NOTIFS       32   CACHE STRING "Max notification objects")
set(UL_CONFIG_MAX_IRQ_BINDINGS 16   CACHE STRING "Max IRQ-to-notif bindings")
set(UL_CONFIG_TICK_HZ          1000 CACHE STRING "Scheduler tick rate in Hz")

configure_file(
    cmake/config.h.in
    ${CMAKE_BINARY_DIR}/include/ul/config.h
    @ONLY
)
```

Override on the command line:

```bash
cmake -B build \
    -DUL_CHIP_DIR=boards/qemu_tc27x \
    -DUL_CONFIG_MAX_THREADS=16 \
    -DUL_CONFIG_TICK_HZ=500
```

### 9.4 Generated header

`cmake/config.h.in` template:

```c
/* include/ul/config.h — generated by CMake; do not edit */

#ifndef UL_CONFIG_H
#define UL_CONFIG_H

#define UL_CONFIG_MAX_THREADS       @UL_CONFIG_MAX_THREADS@
#define UL_CONFIG_MAX_ENDPOINTS     @UL_CONFIG_MAX_ENDPOINTS@
#define UL_CONFIG_MAX_NOTIFS        @UL_CONFIG_MAX_NOTIFS@
#define UL_CONFIG_MAX_IRQ_BINDINGS  @UL_CONFIG_MAX_IRQ_BINDINGS@
#define UL_CONFIG_TICK_HZ           @UL_CONFIG_TICK_HZ@

#endif /* UL_CONFIG_H */
```

The kernel includes this header in its internal implementation files only.
It is **not** part of the public API (`include/ul/microkernel.h` does not include
it). User code does not need to know the kernel's static pool sizes.

### 9.5 What is NOT configurable

| Candidate | Why it is fixed |
|-----------|----------------|
| Thread priority range (0–255) | Fixed by `uint8_t`; 256 levels is more than enough for embedded |
| IPC message word count | Fixed by syscall ABI; changing it would break the calling convention |
| MPU region alignment | Hardware requirement; defined in `arch_config.h` |
| Thread name length (15 chars) | Fixed at 16 bytes (`char name[16]`) per TCB |
| Boot memory region count (4) | Matches TC2xx physical regions: DSPR0/1/2 + LMU |
| `UL_ARCH_MAX_REGIONS` (12) | Conservative fixed value; costs 12 × 12 bytes per TCB |

---

## 10. Phase 2 — CMake Package Export  

When the project matures, the kernel can be installed as a CMake package,
allowing app projects to be independent CMake builds:

```bash
cmake --install build --prefix /opt/ulipeMicroKernel
```

An app project:

```cmake
# my_driver/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
find_package(UlipeMicroKernel REQUIRED
    HINTS $ENV{UL_BASE} /opt/ulipeMicroKernel)
project(my_driver C ASM)

ul_add_app(NAME my_driver SOURCES my_driver.c DOMAIN my_driver STACK 2048 PRIV DRIVER)
ul_add_domain(NAME my_driver PERMS "UL_PERM_READ | UL_PERM_WRITE | UL_PERM_USER")
ul_set_root_thread(SOURCE root_thread.c)
ul_generate_linker_script()

add_executable(ulipe.elf)
ul_finalize_elf(ulipe.elf)
```

In Phase 2, the app project is the top-level CMake build. The kernel is a
dependency. `ul_finalize_elf()` performs the PRE_LINK script generation and
sets linker options on the target.

Phase 2 requires the kernel's `CMakeFiles/Export/` infrastructure and does not
change any of the kernel source or linker fragments — only the CMake packaging.

---

## 11. Bootloader Boundary

`ul_board_init()` (§8.0) eliminates the need for a separate bootloader in many
systems. For chips where the internal oscillator provides enough speed to run PLL
init code (most automotive MCUs, including TC2xx), `ul_board_init()` is
sufficient.

```
WITHOUT external bootloader (common case):

  power-on / reset vector
       │
  _start (startup.S)
       ├─ stack, ISP, BTV/BIV, small-data, CSA
       ├─ ul_board_init()   ← PLL, flash WS, ext RAM
       ├─ .data copy + .bss zero
       ├─ ul_arch_init()
       └─ ul_kernel_main()

WITH external bootloader (when needed):

  power-on → ROM bootloader / Stage-0 (not this repo)
       │  PLL, clock, DRAM, flash WS already configured
       │  jump to _start
  _start (startup.S)
       ├─ stack, ISP, BTV/BIV, small-data, CSA
       ├─ ul_board_init()   ← no-op stub (HW already set up)
       ├─ .data copy + .bss zero
       ├─ ul_arch_init()
       └─ ul_kernel_main()
```

The kernel build system is agnostic to which path is taken. The `ul_board_init()`
weak stub makes both modes work without any build-time flag.

---

## 12. Implementation Roadmap

In dependency order:

| Step | Deliverable | Depends on |
|------|-------------|-----------|
| 1 | `cmake/toolchain-tricore-gcc.cmake` | — |
| 2 | `cmake/config.cmake` + `cmake/config.h.in` — 5 config symbols | — |
| 3 | `boards/qemu_tc27x/memory.ld` | linker spec §10 |
| 4 | `cmake/linker_api.cmake` — `ul_add_app`, `ul_add_domain`, `ul_set_root_thread` | — |
| 5 | `cmake/generate_ld.py` — assembles `generated.ld` from fragments + lists | step 4 |
| 6 | `stub/root_thread_stub.c` + `stub/board_init_stub.c` | API spec §5, arch spec §11 |
| 7 | Top-level `CMakeLists.txt` — config, sibling discovery, ELF target | steps 1–6 |
| 8 | `linker/kernel/*.ld.in` — 6 kernel fragments | linker spec §4 |
| 9 | `arch/tricore/linker/*.ld.in` — prologue, csa_pool, small_data | linker spec §4.5–4.6 |
| 10 | `include/ul/linker.h` — C macro API | linker spec §8 |
| 11 | Kernel source skeleton (`kernel/`, `arch/tricore/`) | arch API spec |
| 12 | Phase 2: `UlipeMicroKernelConfig.cmake.in` + install rules | step 7 |

Steps 1–7 are the minimum to produce a compilable kernel-only ELF. Steps 8–10 complete
the linker infrastructure. Step 11 begins actual kernel implementation.

---

*This document specifies the build system model. Implementation lives in `cmake/`,
`boards/`, `stub/`, and the top-level `CMakeLists.txt`. The generated kernel
configuration header is `${CMAKE_BINARY_DIR}/include/ul/config.h`.*

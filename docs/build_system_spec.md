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
9. [Phase 2 — CMake Package Export](#9-phase-2--cmake-package-export)
10. [Bootloader Boundary](#10-bootloader-boundary)
11. [Implementation Roadmap](#11-implementation-roadmap)

---

## 1. Design Philosophy

The build system follows the same layering principle as the linker and arch specs:
**each layer owns its concerns, nothing bleeds across boundaries**.

```
┌──────────────────────────────────────────────────────────────────────┐
│  ulipeMicroKernel/          Kernel repo — owns kernel source,        │
│                             arch port, linker fragments, CMake API,  │
│                             boards/, stub root_thread.                │
├──────────────────────────────────────────────────────────────────────┤
│  ulipeMicroKernel_apps/     Apps repo (sibling, optional) — owns     │
│                             drivers, application tasks, and the real  │
│                             ul_root_thread() implementation.          │
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
│   └── root_thread_stub.c               ← fallback ul_root_thread() for kernel-only builds
├── boards/                              ← chip inputs (Layer 3 per linker spec)
│   ├── qemu_tc27x/
│   │   └── memory.ld                    ← MEMORY block + flags for QEMU
│   └── tc27x/
│       ├── memory.ld                    ← MEMORY block + flags for TC27x hardware
│       └── bmhd.ld.in                   ← TC27x boot mode header section
├── tests/
└── tools/
    ├── dev.py
    ├── docker/
    └── hello/                           ← standalone hello-world (not part of main build)
```

The apps repository sits **beside** the kernel repository:

```
ulipeMicroKernel/            ← kernel repo
ulipeMicroKernel_apps/       ← apps repo (sibling, optional)
    CMakeLists.txt
    root_thread.c
    drivers/
        asclin/
            CMakeLists.txt
            asclin.c
    apps/
        sensor/
            CMakeLists.txt
            sensor.c
```

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
│   └─ absent → stub root_thread registered
│
├─ ul_generate_linker_script():
│   registers PRE_LINK command:
│     generate_ld.py → build/generated.ld
│
└─ add_executable(ulipe.elf
       kernel/**/*.c
       arch/tricore/startup.S
       arch/tricore/vectors.S
       <app sources registered by ul_add_app>
       <root_thread.c or stub>)
   target_link_options(-T build/generated.ld)
   target_include_directories(include/)

cmake --build build
│
├─ compile kernel sources
├─ compile arch sources
├─ compile app sources (each with -DUL_APP_NAME=<name>)
├─ compile root_thread
├─ [PRE_LINK] generate_ld.py → build/generated.ld
└─ link → build/ulipe.elf
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

## 8. Root Thread and Stub

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

## 9. Phase 2 — CMake Package Export

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

## 10. Bootloader Boundary

The current model assumes the CPU starts executing at `_start` in `startup.S`
with hardware in its reset state. Clock configuration, PLL setup, and flash
wait-state tuning are **not** the kernel's responsibility. A first-stage
bootloader may handle these before passing control to `_start`.

```
┌─────────────────────┐  ← power-on / reset vector
│  Stage 0 (optional) │     ROM bootloader or first-stage (not this repo)
│  - PLL / clock init │
│  - Flash wait states│
│  - DRAM init        │
└────────┬────────────┘
         │ jump to _start
┌────────▼────────────┐
│  startup.S (_start) │     this repo
│  - SP / ISP         │
│  - BTV / BIV        │
│  - small data regs  │
│  - CSA pool init    │
│  - .data copy       │
│  - .bss zero        │
│  → ul_arch_init()   │
│  → ul_kernel_main() │
└─────────────────────┘
```

This boundary means the kernel and its build system are agnostic to clock
sources, DRAM configuration, and any other chip bring-up details. Those belong
in the bootloader or in the chip input package (`boards/<chip>/`), not in the
kernel build.

---

## 11. Implementation Roadmap

In dependency order:

| Step | Deliverable | Depends on |
|------|-------------|-----------|
| 1 | `cmake/toolchain-tricore-gcc.cmake` | — |
| 2 | `boards/qemu_tc27x/memory.ld` | linker spec §9 |
| 3 | `cmake/linker_api.cmake` — `ul_add_app`, `ul_add_domain`, `ul_set_root_thread` | — |
| 4 | `cmake/generate_ld.py` — assembles `generated.ld` from fragments + lists | step 3 |
| 5 | `stub/root_thread_stub.c` | API spec §5 |
| 6 | Top-level `CMakeLists.txt` — sibling discovery, ELF target | steps 1–5 |
| 7 | `linker/kernel/*.ld.in` — 6 kernel fragments | linker spec §4 |
| 8 | `arch/tricore/linker/*.ld.in` — prologue, csa_pool, small_data | linker spec §4.5–4.6 |
| 9 | `include/ul/linker.h` — C macro API | linker spec §8 |
| 10 | Kernel source skeleton (`kernel/`, `arch/tricore/`) | arch API spec |
| 11 | Phase 2: `UlipeMicroKernelConfig.cmake.in` + install rules | step 6 |

Steps 1–6 are the minimum to produce a compilable kernel-only ELF. Steps 7–9 complete
the linker infrastructure. Step 10 begins actual kernel implementation.

---

*This document specifies the build system model. Implementation lives in `cmake/`,
`boards/`, `stub/`, and the top-level `CMakeLists.txt`.*

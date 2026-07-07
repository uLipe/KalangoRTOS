# ulmk

A small, seL4-inspired microkernel for automotive-grade embedded systems.
Primary targets: **AURIX TriCore TC2xx/TC3xx** (QEMU CI) and **RISC-V RV32IMAC**
(QEMU `virt` CI).  Isolation is enforced by the hardware MPU/PMP at thread
granularity; policy lives entirely in userspace.  Single ELF output, O(1) bitmap
scheduler, synchronous IPC with priority inheritance.

---

## Architecture overview

```
┌─────────────────────────────────────────────────┐
│  ulmk_root_thread()                               │
│  board_services (console, clocks, …)            │  userspace
│  components (hello_world, drivers, apps, …)     │  (ULMK_PRIV_DRIVER / USER)
├─────────────────────────────────────────────────┤
│  kernel/  — scheduler, IPC, memory, IRQ table   │  supervisor
│  arch/    — context switch, MPU, tick, atomics  │
├─────────────────────────────────────────────────┤
│  board/   — ulmk_board_init, ulmk_printk_char_out   │  hardware
│  chip     — MEMORY block (external, ULMK_CHIP_DIR)│
└─────────────────────────────────────────────────┘
```

Key properties:

- **One ELF, MPU isolation.**  All components link into a single binary.
  The hardware MPU enforces data domain boundaries between threads.
- **Synchronous IPC.**  Caller blocks until server replies.  Priority
  inheritance prevents priority inversion.
- **Component model.**  Each feature is a component: a directory with a
  `CMakeLists.txt` calling `ulmk_component_register()`.  Components default to
  **OFF**; enable them with `python3 tools/dev.py components enable`.
- **No weak symbols.**  Board and component sources provide strong definitions.
  A missing symbol is a link error, not a silent no-op.

---

## Application model

```
ulmk_root_thread()                 ← provided by the ROOT_THREAD component
    board_services_init(info)    ← provided by the board (console, etc.)
    my_component_init()          ← provided by each enabled component
    ulmk_thread_exit()
```

See `docs/application_development_guide.md` for a complete walkthrough.

---

## Getting started

### Requirements

- Linux host (Ubuntu 22.04 or newer recommended)
- Docker (for the dev container — cross-toolchains + QEMU)
- Python 3.8+

Toolchains and QEMU are only available inside the dev container.  Do **not**
try to compile or run target tests on the host.

### Enter the dev container

```bash
# First run builds the Docker image (~20–30 min)
python3 tools/dev.py

# Force rebuild if the image is stale
python3 tools/dev.py --rebuild
```

The workspace is mounted at `/workspace` inside the container.

### Build the hello world demo (inside the container)

All components are **OFF by default**. Enable the demo stack, build, and run:

```bash
# 1. See what is available
python3 tools/dev.py components list

# 2. Enable hello_world (requires ping_pong — enable both)
python3 tools/dev.py components enable hello_world ping_pong

# 3. Build
python3 tools/dev.py build --board boards/qemu_riscv_virt

# TriCore QEMU (default board)
python3 tools/dev.py build

# Clean rebuild
python3 tools/dev.py build --clean

# One-shot enable without saving .ulmk/components.conf
python3 tools/dev.py build --component hello_world --component ping_pong

# Kernel-only image (no components)
python3 tools/dev.py build --no-components
```

Local component selection is stored in `.ulmk/components.conf` (gitignored).

### Run on QEMU (inside the container)

```bash
python3 tools/dev.py build qemu --board boards/qemu_riscv_virt
python3 tools/dev.py build qemu
```

Expected output (with demo components enabled):

```
ulmk: kernel entry
...
ulmk: switching to root thread
ulmk: hello from userspace — tick #0
ping_pong: round 1
```

### Alternative: consume ulmk as a prebuilt SDK

The demo above builds a single ELF from the kernel sources.  If instead you want
to drop ulmk into an **existing firmware tree or a third-party IDE** (Eclipse,
STM32Cube, the Infineon toolchain — e.g. to replace FreeRTOS), build it once as a
distributable SDK: two static archives + a fully-processed linker script + the
public headers.

```bash
# --board is mandatory with --kernel; it may point anywhere (out-of-tree board)
python3 tools/dev.py build --kernel --board boards/qemu_tc3xx
```

This emits a self-contained tree under `build/ulipe-<arch>-sdk/dist/ulmk/`:

```
ulmk/
  lib/ulmk_kernel_<tag>.a     kernel + arch (supervisor)
  lib/ulmk_board_<tag>.a      startup + vectors + board services (driver)
  linker/linker_<tag>.ld      processed linker script
  include/                    public microkernel + board headers
```

Your firmware then provides `ulmk_root_thread()`, includes `<ulmk/microkernel.h>`,
and links both archives.  See the
[SDK integration guide](docs/application_development_guide.md#14-sdk-mode--integrating-ulmk-into-an-external-toolchain)
for the full recipe, and `tests/sdk_e2e/` for a working consumer.

### Build options reference

```bash
# Custom board chip dir
python3 tools/dev.py build --board /path/to/my_board
```

CMake configure variables of interest:

```bash
-DULMK_CHIP_DIR=boards/qemu_tc3xx           # TriCore QEMU (default)
-DULMK_CHIP_DIR=boards/qemu_riscv_virt      # RISC-V QEMU virt
-DULMK_COMP_hello_world_ENABLED=ON          # component override (dev.py sets these)
-DULMK_CONFIG_MAX_IRQ_BINDINGS=16          # SRPN → notif binding table
-DULMK_CONFIG_DEBUG_PRINTK=1               # kernel debug prints
```

### Run tests (inside the container)

```bash
# Unit tests (host, no QEMU)
python3 tools/dev.py tests unit

# Integration tests — TriCore (default)
python3 tools/dev.py tests integ

# Integration tests — RISC-V
python3 tools/dev.py tests integ --board boards/qemu_riscv_virt
```

Individual integration tests also support `ARCH=tricore` or `ARCH=riscv` via
`tests/integ_common.mk` (see any `tests/*/Makefile`).

---

## Repository layout

```
CMakeLists.txt               top-level build orchestrator
cmake/
  arch.cmake                   ULMK_ARCH selection from board.cmake
  toolchain-tricore-gcc.cmake
  toolchain-riscv-gcc.cmake
  component_api.cmake          ulmk_component_register, ulmk_components_finalize
  config.cmake                 kernel configuration symbols
  linker_api.cmake             ulmk_generate_linker_script
  generate_ld.py               assembles the generated linker script
kernel/                      platform-independent kernel
arch/tricore/                TriCore TC1.6.x port
arch/riscv/                  RISC-V RV32 port
boards/qemu_tc3xx/           TriCore QEMU CI board
boards/qemu_riscv_virt/      RISC-V QEMU virt CI board
components/hello_world/      reference ROOT_THREAD component (default OFF)
components/ping_pong/        IPC ping/pong demo (default OFF)
include/ulmk/microkernel.h     public API (all syscall wrappers)
linker/                      arch-independent linker fragments
stub/                        documentation-only stub templates
tests/                       integration tests (standalone Makefiles)
tools/dev.py                 container frontend
docs/                        specifications and guides
```

---

## Documentation

| Document | What it covers |
|----------|----------------|
| [api\_spec](docs/api_spec.md) | Complete public API reference |
| [arch\_api\_spec](docs/arch_api_spec.md) | Architecture abstraction layer contract |
| [build\_system\_spec](docs/build_system_spec.md) | CMake build model and component discovery |
| [component\_spec](docs/component_spec.md) | Component system design |
| [linker\_spec](docs/linker_spec.md) | Three-layer linker script model |
| [application\_development\_guide](docs/application_development_guide.md) | How to build an application for custom hardware |
| [arch\_porting\_guide](docs/arch_porting_guide.md) | How to add a new architecture |
| [riscv\_implementation](docs/arch/riscv_implementation.md) | RISC-V RV32 port details |

---

## License

MIT — see `SPDX-License-Identifier: MIT` in each source file.

# ulipeMicroKernel

A small, seL4-inspired microkernel for automotive-grade embedded systems,
targeting AURIX TriCore TC2xx/TC3xx.  Isolation is enforced by the hardware MPU
at thread granularity; policy lives entirely in userspace.  Single ELF output,
O(1) bitmap scheduler, synchronous IPC with priority inheritance.

---

## Architecture overview

```
┌─────────────────────────────────────────────────┐
│  ul_root_thread()                               │
│  board_services (console, clocks, …)            │  userspace
│  components (hello_world, drivers, apps, …)     │  (UL_PRIV_DRIVER / USER)
├─────────────────────────────────────────────────┤
│  kernel/  — scheduler, IPC, memory, IRQ table   │  supervisor
│  arch/    — context switch, MPU, tick, atomics  │
├─────────────────────────────────────────────────┤
│  board/   — ul_board_init, ul_printk_char_out   │  hardware
│  chip     — MEMORY block (external, UL_CHIP_DIR)│
└─────────────────────────────────────────────────┘
```

Key properties:

- **One ELF, MPU isolation.**  All components link into a single binary.
  The hardware MPU enforces data domain boundaries between threads.
- **Synchronous IPC.**  Caller blocks until server replies.  Priority
  inheritance prevents priority inversion.
- **Component model.**  Each feature is a component: a directory with a
  `CMakeLists.txt` calling `ul_component_register()`.  The build auto-discovers
  components in `components/` and in the optional `../ulmk_apps/` sibling.
- **No weak symbols.**  Board and component sources provide strong definitions.
  A missing symbol is a link error, not a silent no-op.

---

## Application model

```
ul_root_thread()                 ← provided by the ROOT_THREAD component
    board_services_init(info)    ← provided by the board (console, etc.)
    my_component_init()          ← provided by each enabled component
    ul_thread_exit()
```

See `docs/application_development_guide.md` for a complete walkthrough.

---

## Getting started

### Requirements

- Linux host (Ubuntu 22.04 or newer recommended)
- Docker (for the dev container — TriCore toolchain + QEMU AURIX fork)
- Python 3.8+

The TriCore GCC cross-compiler and the QEMU AURIX fork are only available
inside the dev container.  Do **not** try to compile or run tests on the host.

### Enter the dev container

```bash
# First run builds the Docker image (~20–30 min)
python3 tools/dev.py

# Force rebuild if the image is stale
python3 tools/dev.py --rebuild
```

The workspace is mounted at `/workspace` inside the container.

### Build (inside the container)

```bash
# Build for QEMU (default board)
python3 tools/dev.py build

# Clean build
python3 tools/dev.py build --clean

# Custom board chip dir
python3 tools/dev.py build --board /path/to/my_board
```

CMake configure variables of interest:

```bash
-DUL_CHIP_DIR=boards/qemu_tc3xx           # board selection (default)
-DUL_CONFIG_MAX_THREADS=32                # TCB pool size
-DUL_CONFIG_HW_SYS_CLOCK_HZ=50000000     # system clock (Hz); match your board
-DUL_CONFIG_TICK_HZ=1000                  # scheduler tick rate
```

### Run on QEMU (inside the container)

```bash
python3 tools/dev.py run
```

Expected output:

```
ulipeMicroKernel: kernel entry
[DBG] sched init done
...
ulipeMicroKernel: switching to root thread
ulipeMicroKernel: hello from userspace — tick #0
ulipeMicroKernel: hello from userspace — tick #1
...
```

### Run integration tests (inside the container)

```bash
cd /workspace/tests/boot          && make clean && make gen_config && make run
cd /workspace/tests/ctx_switch    && make clean && make gen_config && make run
cd /workspace/tests/sleep_integ   && make clean && make gen_config && make run
cd /workspace/tests/sched_integ   && make clean && make gen_config && make run
cd /workspace/tests/ipc_integ     && make clean && make gen_config && make run
cd /workspace/tests/thread_lifecycle_integ && make clean && make gen_config && make run
cd /workspace/tests/resource_leak_integ    && make clean && make gen_config && make run
cd /workspace/tests/preempt_integ && make clean && make gen_config && make run
```

All 8 tests must pass before submitting a patch.

---

## Repository layout

```
CMakeLists.txt               top-level build orchestrator
cmake/
  toolchain-tricore-gcc.cmake
  component_api.cmake          ul_component_register, ul_components_finalize
  config.cmake                 6 kernel configuration symbols
  linker_api.cmake             ul_generate_linker_script
  generate_ld.py               assembles the generated linker script
kernel/                      platform-independent kernel
arch/tricore/                TriCore TC1.6.x port
boards/qemu_tc3xx/           QEMU AURIX TC397B board (CI platform)
components/hello_world/      reference component (ROOT_THREAD)
include/ul/microkernel.h     public API (all syscall wrappers)
linker/                      arch-independent linker fragments
stub/                        documentation-only stub templates
tests/                       integration tests (standalone Makefiles)
tools/dev.py                 container frontend
docs/                        specifications and guides
```

---

## Documentation

| Document | What it covers |
|----------|---------------|
| `docs/api_spec.md` | Complete public API reference |
| `docs/arch_api_spec.md` | Architecture abstraction layer contract |
| `docs/build_system_spec.md` | CMake build model and component discovery |
| `docs/component_spec.md` | Component system design |
| `docs/linker_spec.md` | Three-layer linker script model |
| `docs/application_development_guide.md` | How to build an application for custom hardware |
| `docs/arch_porting_guide.md` | How to add a new architecture |
| `docs/microkernel_book_tricore.md` | TriCore microkernel implementation reference |
| `docs/tricore_guide_pt.md` | TriCore ABI, CSA, context switch deep-dive |

---

## License

MIT — see `SPDX-License-Identifier: MIT` in each source file.

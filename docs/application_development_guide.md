# ulipeMicroKernel — Application Development Guide

> **Purpose of this document:** step-by-step guide for building an application
> on top of ulipeMicroKernel, targeting custom hardware.  Covers the component
> model, what files to create, how to wire the board, and how to run on QEMU
> or real hardware.

---

## Table of Contents

1. [Concepts](#1-concepts)
2. [Repository Layout for an Application](#2-repository-layout-for-an-application)
3. [Creating a Component](#3-creating-a-component)
4. [Creating the Root Thread](#4-creating-the-root-thread)
5. [Providing Board Services](#5-providing-board-services)
6. [Creating a Custom Board (chip input)](#6-creating-a-custom-board-chip-input)
7. [Build](#7-build)
8. [Output Artefacts](#8-output-artefacts)
9. [Running on QEMU](#9-running-on-qemu)
10. [Running on Real Hardware](#10-running-on-real-hardware)
11. [Worked Example](#11-worked-example)

---

## 1. Concepts

### Single ELF, MPU isolation

The entire system — kernel, board services, all application components — links
into **one ELF file**.  Isolation is enforced at runtime by the hardware MPU,
not by separate binaries.

### Component

A directory containing a `CMakeLists.txt` that calls `ul_component_register()`.
Its sources are compiled into the `ulipe_kernel` static library.  Its public
header is added to the global include path.

Exactly one component must declare `ROOT_THREAD`.  That component provides
`ul_root_thread()`, which is the first userspace function the kernel calls.

### Board services

Hardware-dependent services (console, clocks, peripherals) live in the board
directory, not in components.  The board provides three mandatory symbols:

| Symbol | Called from | What it does |
|--------|-------------|-------------|
| `ul_board_init(void)` | `startup.S` before `.data` copy | PLL, flash WS, ext RAM |
| `ul_printk_char_out(char)` | kernel printk | single-character debug output |
| `board_services_init(const ul_boot_info_t *)` | `ul_root_thread()` | spawn background service threads |

---

## 2. Repository Layout for an Application

```
my_project/
├── ulmk_apps/                ← auto-discovered sibling of ulipeMicroKernel/
│   ├── my_app/               ← application component (ROOT_THREAD)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── my_app.h
│   │   └── src/
│   │       ├── root_thread.c
│   │       └── my_task.c
│   └── my_driver/            ← driver component
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── my_driver.h
│       └── src/
│           ├── server.c
│           └── client.c
│
├── ulipeMicroKernel/         ← kernel repo (git submodule or clone)
│
└── my_board/                 ← chip input for real hardware
    ├── board.cmake
    ├── memory.ld
    ├── board_console.c       ← ul_printk_char_out via UART
    └── board_services.c      ← ul_board_init + board_services_init
```

Place `ulmk_apps/` as a sibling of `ulipeMicroKernel/`.  The build
auto-discovers it:

```cmake
# From CMakeLists.txt — happens automatically
if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/../ulmk_apps")
    file(GLOB dirs LIST_DIRECTORIES true "../ulmk_apps/*")
    foreach(dir IN LISTS dirs)
        add_subdirectory(${dir} ...)
    endforeach()
endif()
```

---

## 3. Creating a Component

### Minimum file set

```
my_component/
├── CMakeLists.txt
├── include/
│   └── my_component.h    ← public C API
└── src/
    ├── server.c           ← IPC receive loop (if this is a service)
    └── client.c           ← public API wrappers calling IPC
```

### CMakeLists.txt

```cmake
ul_component_register(
    NAME         my_component
    ENABLED      ON
    SOURCES      src/server.c
                 src/client.c
    INCLUDE_DIRS include
)
```

Flags:
- `ROOT_THREAD` — set on exactly one component; that component provides
  `ul_root_thread()`.
- `REQUIRES other_component` — build fails if `other_component` is DISABLED.
- `LINKER_FRAGMENT my_component.ld.in` — optional memory domain fragment.
- `ENABLED OFF` — exclude from build without removing the directory.

### Public header convention

```c
/* include/my_component.h */
#ifndef MY_COMPONENT_H
#define MY_COMPONENT_H

#include <ul/microkernel.h>

/*
 * my_component_init — spawn the service thread and create the IPC endpoint.
 * The endpoint is ready before this function returns; callers can invoke the
 * service API immediately.  Returns UL_TID_INVALID on double-init.
 */
ul_tid_t my_component_init(void);

/* Service API — IPC details are an internal implementation detail. */
int my_component_do_thing(int arg);

#endif
```

### Service pattern (server + client)

```c
/* src/server.c */
#include <my_component.h>
#include <ul/microkernel.h>

#define MSG_DO_THING  1u

static ul_ep_t g_ep;

static void server_thread(void *arg)
{
    ul_msg_t msg;
    ul_tid_t sender;

    (void)arg;
    for (;;) {
        ul_ep_recv(g_ep, &msg, &sender);
        /* process msg.label, msg.words[] */
        ul_msg_t reply = { .label = UL_OK };
        ul_ep_reply(sender, &reply);
    }
}

ul_tid_t my_component_init(void)
{
    static int done;
    if (done)
        return UL_TID_INVALID;
    done = 1;

    g_ep = ul_ep_create();   /* endpoint ready before thread starts */

    ul_thread_attr_t attr = {
        .name       = "my_comp",
        .entry      = server_thread,
        .arg        = NULL,
        .priority   = 10,
        .stack_size = 1024,
        .privilege  = UL_PRIV_DRIVER,
    };
    return ul_thread_create(&attr);
}
```

```c
/* src/client.c */
#include <my_component.h>
#include <ul/microkernel.h>

extern ul_ep_t g_ep;  /* defined in server.c */

int my_component_do_thing(int arg)
{
    ul_msg_t msg = { .label = MSG_DO_THING, .words = { (uint32_t)arg } };
    int ret = ul_ep_call(g_ep, &msg);
    return ret == UL_OK ? (int)msg.words[0] : ret;
}
```

---

## 4. Creating the Root Thread

The root thread component declares `ROOT_THREAD` and provides `ul_root_thread()`.

```cmake
# ulmk_apps/my_app/CMakeLists.txt
ul_component_register(
    NAME         my_app
    ENABLED      ON
    SOURCES      src/root_thread.c
                 src/main_task.c
    INCLUDE_DIRS include
    ROOT_THREAD
)
```

```c
/* src/root_thread.c */
#include <ul/microkernel.h>
#include <board_services.h>   /* provided by the board */
#include <my_driver.h>
#include <my_app.h>

void ul_root_thread(const ul_boot_info_t *info)
{
    /*
     * 1. Board services: console, clocks, etc.
     *    board_services_init() creates endpoints and spawns board threads
     *    before returning.  Service APIs are safe to call immediately after.
     */
    board_services_init(info);

    /*
     * 2. Components, in dependency order.
     *    Each *_init() creates its endpoint and spawns its service thread.
     */
    my_driver_init();
    my_app_init(info);

    /*
     * 3. Optionally delegate capabilities before exiting.
     */
    /* ul_cap_grant(driver_tid, UL_CAP_IRQ | UL_CAP_MAP_PERIPH); */

    ul_thread_exit();
}
```

---

## 5. Providing Board Services

Create a board directory (can be inside `ulmk_apps/` or anywhere on disk):

```
my_board/
├── board.cmake          ← board descriptor
├── memory.ld            ← MEMORY block
├── board_console.c      ← ul_printk_char_out via UART or semihosting
├── board_services.c     ← ul_board_init + board_services_init
└── board_services.h     ← declares board_services_init()
```

### board.cmake

```cmake
set(UL_BOARD_CPU   "tc39xx")   # passed to -mcpu=
set(UL_BOARD_CFLAGS
    "-DUL_ARCH_SRC_STM0_SR0=0xF0038300u"
    "-DUL_ARCH_SRC_SRE_BIT=10u"
    "-DUL_ARCH_IDLE_IS_WAIT=0"
)
set(UL_BOARD_SOURCES
    board_console.c
    board_services.c
)
```

### board_services.c

```c
#include <ul/microkernel.h>
#include "board_services.h"

void ul_board_init(void)
{
    /*
     * Called before .data copy — no globals, no kernel API.
     * Configure PLL, flash wait states, external RAM here.
     * Leave empty if a bootloader already did this.
     */
}

void ul_printk_char_out(char c)
{
    /* Write c to UART TX register or semihosting interface. */
    volatile uint32_t *tx = (volatile uint32_t *)0xF0000020u;
    *tx = (uint32_t)c;
}

void board_services_init(const ul_boot_info_t *info)
{
    (void)info;
    /* Spawn any board-level service threads (console IPC server, etc.). */
}
```

### board_services.h

```c
#ifndef BOARD_SERVICES_H
#define BOARD_SERVICES_H

#include <ul/microkernel.h>

void board_services_init(const ul_boot_info_t *info);

#endif
```

---

## 6. Creating a Custom Board (chip input)

The chip input provides the MEMORY block and linker flags.  See
`docs/linker_spec.md §9` for the full contract.  Minimum:

### memory.ld

```ld
/* memory.ld — chip input for My TC277 board */

UL_MPU_ALIGN    = 64;
UL_KERNEL_STACK_SIZE = 4096;
UL_ISR_STACK_SIZE    = 2048;
UL_CSA_POOL_SIZE     = 16384;  /* 256 × 64-byte CSA frames */

HAVE_CSA        = 1;
HAVE_SMALL_DATA = 1;
HAVE_BMHD       = 1;

MEMORY {
    KERNEL_FLASH    (rx)  : ORIGIN = 0x80000000, LENGTH = 512K
    KERNEL_FLASH_NC (rx)  : ORIGIN = 0xA0000000, LENGTH = 512K  /* BMHD lives here */
    KERNEL_RAM      (rwx) : ORIGIN = 0x70000000, LENGTH = 240K
    SHARED_RAM      (rwx) : ORIGIN = 0x90000000, LENGTH = 32K
    PERIPH          (rw)  : ORIGIN = 0xF0000000, LENGTH = 256M
}
```

---

## 7. Build

```bash
# Enter the dev container
python3 tools/dev.py

# Inside the container — build with custom board
python3 tools/dev.py build --board /workspace/../my_board

# Or directly with cmake
cmake -B /build/my_project \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-tricore-gcc.cmake \
      -DUL_CHIP_DIR=/workspace/../my_board
cmake --build /build/my_project
```

Key CMake options:

| Option | Default | Description |
|--------|---------|-------------|
| `UL_CHIP_DIR` | `boards/qemu_tc3xx` | Path to chip input directory |
| `UL_CONFIG_HW_SYS_CLOCK_HZ` | `50000000` | System clock — **must match your hardware** |
| `UL_CONFIG_MAX_THREADS` | `32` | TCB pool size |
| `UL_CONFIG_TICK_HZ` | `1000` | Scheduler tick rate (Hz) |

---

## 8. Output Artefacts

After a successful build:

```
build/
└── ulipe/
    ├── ulipe_microkernel         ← final ELF
    ├── libulipe_kernel.a         ← kernel + board + component objects
    └── generated/
        └── ulipe_microkernel.ld  ← generated linker script
```

| Artefact | Use |
|----------|-----|
| `ulipe_microkernel` | Flash or load onto target |
| `ulipe_microkernel.ld` | Inspect section layout, debug address issues |
| `libulipe_kernel.a` | Not needed on-target; used by the link step only |

Convert to Intel HEX or binary for flashing:

```bash
tricore-elf-objcopy -O ihex ulipe_microkernel ulipe_microkernel.hex
tricore-elf-objcopy -O binary ulipe_microkernel ulipe_microkernel.bin
```

Inspect section sizes:

```bash
tricore-elf-size ulipe_microkernel
tricore-elf-objdump -h ulipe_microkernel | grep -E "text|data|bss|startup"
```

---

## 9. Running on QEMU

```bash
# Inside the dev container
python3 tools/dev.py run

# Or directly
qemu-system-tricore -machine KIT_AURIX_TC397B_TRB \
    -kernel /build/ulipe/ulipe_microkernel \
    -nographic
```

Press `Ctrl+A X` to exit QEMU.

QEMU limitations:
- Starts execution at `0x80000000` (reset address), not at the ELF entry point.
  The `.startup` linker section ensures `_start` is placed there.
- MMIO console output at `0xBF000020` (VIRT device) — configured in
  `boards/qemu_tc3xx/qemu_console.c`.
- No flash programming — `ulipe_microkernel` is loaded directly as an ELF.

---

## 10. Running on Real Hardware

### Flash via Lauterbach Trace32

```
; trace32 script snippet
Data.LOAD.Elf ulipe_microkernel /NoCODE
FLASH.Program ALL
System.Up
Go
```

### Flash via MemTool / DAS (Infineon)

```bash
# Requires DAS and the appropriate TC2xx/TC3xx device package
infineon-memtool -d TC277 -f ulipe_microkernel.hex
```

### OpenOCD + GDB (for development)

```bash
openocd -f interface/aurix.cfg -f target/tc277.cfg &
tricore-elf-gdb ulipe_microkernel \
    -ex "target remote :3333" \
    -ex "monitor reset halt" \
    -ex "load" \
    -ex "continue"
```

### JTAG boot sequence

After flashing:

1. Power cycle or reset the board.
2. The chip ROM loads the BMHD (if `HAVE_BMHD = 1`) and jumps to `_start`.
3. `ul_board_init()` runs (PLL, flash WS).
4. `.data` copy and `.bss` zero.
5. `ul_arch_init()` — CSA pool, vectors, MPU, tick timer.
6. `ul_kernel_main()` — scheduler starts.
7. `ul_root_thread()` — your application code.

---

## 11. Worked Example

Create a minimal application that blinks an LED via a driver component.

### File structure

```
ulmk_apps/
├── led_blink/          ← ROOT_THREAD component
│   ├── CMakeLists.txt
│   └── src/root_thread.c
└── gpio_driver/        ← driver component
    ├── CMakeLists.txt
    ├── include/gpio_driver.h
    └── src/gpio_driver.c
```

### gpio_driver/CMakeLists.txt

```cmake
ul_component_register(
    NAME         gpio_driver
    ENABLED      ON
    SOURCES      src/gpio_driver.c
    INCLUDE_DIRS include
)
```

### gpio_driver/include/gpio_driver.h

```c
#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include <ul/microkernel.h>

ul_tid_t gpio_driver_init(void);
void     gpio_set(uint8_t pin, int val);

#endif
```

### gpio_driver/src/gpio_driver.c

```c
#include <gpio_driver.h>
#include <ul/microkernel.h>

#define MSG_GPIO_SET  1u

static ul_ep_t g_ep;

static void gpio_server(void *arg)
{
    ul_msg_t msg;
    ul_tid_t sender;

    (void)arg;
    for (;;) {
        ul_ep_recv(g_ep, &msg, &sender);
        if (msg.label == MSG_GPIO_SET) {
            volatile uint32_t *port = (volatile uint32_t *)0xF003A000u;
            if (msg.words[1])
                *port |= (1u << msg.words[0]);
            else
                *port &= ~(1u << msg.words[0]);
        }
        ul_msg_t reply = { .label = UL_OK };
        ul_ep_reply(sender, &reply);
    }
}

ul_tid_t gpio_driver_init(void)
{
    static int done;
    if (done)
        return UL_TID_INVALID;
    done = 1;
    g_ep = ul_ep_create();
    ul_thread_attr_t attr = {
        .name = "gpio", .entry = gpio_server, .arg = NULL,
        .priority = 5, .stack_size = 512, .privilege = UL_PRIV_DRIVER,
    };
    return ul_thread_create(&attr);
}

void gpio_set(uint8_t pin, int val)
{
    ul_msg_t msg = { .label = MSG_GPIO_SET, .words = { pin, (uint32_t)val } };
    ul_ep_call(g_ep, &msg);
}
```

### led_blink/CMakeLists.txt

```cmake
ul_component_register(
    NAME         led_blink
    ENABLED      ON
    SOURCES      src/root_thread.c
    INCLUDE_DIRS include
    REQUIRES     gpio_driver
    ROOT_THREAD
)
```

### led_blink/src/root_thread.c

```c
#include <ul/microkernel.h>
#include <board_services.h>
#include <gpio_driver.h>

static void blink_task(void *arg)
{
    (void)arg;
    for (;;) {
        gpio_set(0, 1);
        ul_timer_set_deadline(500000);  /* 500 ms */
        ul_timer_wait();
        gpio_set(0, 0);
        ul_timer_set_deadline(500000);
        ul_timer_wait();
    }
}

void ul_root_thread(const ul_boot_info_t *info)
{
    board_services_init(info);
    gpio_driver_init();

    ul_thread_attr_t attr = {
        .name = "blink", .entry = blink_task, .arg = NULL,
        .priority = 20, .stack_size = 512, .privilege = UL_PRIV_DRIVER,
    };
    ul_tid_t tid = ul_thread_create(&attr);
    ul_cap_grant(tid, UL_CAP_TIMER);

    ul_thread_exit();
}
```

### Build and run

```bash
# Build with the custom board
python3 tools/dev.py build --board /workspace/../my_board

# Flash or run on QEMU
python3 tools/dev.py run
```

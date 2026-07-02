# ulipeMicroKernel — Component System Specification

**Version:** 0.1 (draft)
**Status:** Awaiting review before implementation

---

## Table of Contents

1. [Design Philosophy](#1-design-philosophy)
2. [Component Directory Structure](#2-component-directory-structure)
3. [CMake Registration API](#3-cmake-registration-api)
4. [Discovery and Scan Rules](#4-discovery-and-scan-rules)
5. [Build Flow](#5-build-flow)
6. [Linker Fragment Contract](#6-linker-fragment-contract)
7. [Root Thread Ownership](#7-root-thread-ownership)
8. [Component Interactions — IPC Contract](#8-component-interactions--ipc-contract)
9. [hello_world Component Design](#9-hello_world-component-design)
10. [board_console Component Design](#10-board_console-component-design)
11. [Open Questions](#11-open-questions)

---

## 1. Design Philosophy

A **component** is an independently-developed, reusable unit of userspace code that runs inside the microkernel image alongside other components. It is the primary packaging unit for drivers, services, and applications built on top of the microkernel.

Key principles:

- **One image, N components.** The microkernel produces a single ELF. All components are compiled and linked into that ELF. Isolation is enforced at runtime by the MPU, not by separate binaries.
- **Components are opt-in.** Every component declares itself as `ENABLED` or `DISABLED` in its CMake descriptor. Disabled components contribute zero code to the image.
- **Only one root thread.** The kernel creates exactly one initial userspace context. Exactly one component provides the `ul_root_thread()` function. That component is responsible for starting all other components' service threads.
- **Kernel knows nothing about components.** Component names, endpoints, and initialisation order are purely userspace policy. The kernel only sees threads, endpoints, and memory mappings.
- **Components communicate through IPC.** Services exposed by one component to others are accessed via IPC endpoints, not via direct function calls. This enforces isolation even before hardware MPU limits are enforced.

---

## 2. Component Directory Structure

```
my_component/
├── CMakeLists.txt          # component descriptor; calls ul_component_register()
├── include/
│   └── my_component.h      # public API header (exposed to other components)
├── src/
│   ├── server.c            # service thread entry point(s)
│   └── client.c            # client library (calls into IPC endpoint)
└── linker.ld.in            # (optional) domain memory fragment
```

### Rules

- The `CMakeLists.txt` **must** contain exactly one `ul_component_register()` call.
- `include/` headers are added to the global include path for all components in the same build.
- Sources listed in `SOURCES` are compiled with `-DUL_COMPONENT_NAME=<name>` defined.
- `linker.ld.in` is optional. If present and the component is enabled, it is appended to the generated linker script.

---

## 3. CMake Registration API

### `ul_component_register`

Declared in `cmake/component_api.cmake`, included automatically by the build system.

```cmake
ul_component_register(
    NAME          <string>          # component identifier; must be unique
    ENABLED       <ON|OFF>          # whether this component participates in the build
    SOURCES       <files…>          # C/ASM sources relative to component dir
    INCLUDE_DIRS  <dirs…>           # directories added to the public include path
    REQUIRES      <names…>          # other component names this one depends on
    ROOT_THREAD                     # (optional flag) this component provides ul_root_thread()
    LINKER_FRAGMENT <file>          # (optional) .ld.in fragment for memory domains
)
```

**Semantics:**

| Field | Required | Notes |
|---|---|---|
| `NAME` | yes | Identifier used in `REQUIRES` lists and in `-DUL_COMPONENT_NAME=<name>` |
| `ENABLED` | yes | If `OFF`, the component is entirely skipped — no sources, no headers, no linker fragment |
| `SOURCES` | yes | Paths relative to the component's own directory |
| `INCLUDE_DIRS` | no | Exposed to all other enabled components |
| `REQUIRES` | no | Order and dependency hints; does not yet enforce link ordering |
| `ROOT_THREAD` | no (flag) | Marks this component as the one providing `ul_root_thread()` |
| `LINKER_FRAGMENT` | no | Path relative to component dir; appended verbatim to the generated linker script |

### Example

```cmake
# components/hello_world/CMakeLists.txt
ul_component_register(
    NAME         hello_world
    ENABLED      ON
    SOURCES
        src/root_thread.c
        src/hello_world.c
        src/console.c
    INCLUDE_DIRS include
    REQUIRES     board_console
    ROOT_THREAD
    LINKER_FRAGMENT linker.ld.in
)
```

---

## 4. Discovery and Scan Rules

The build system scans the following locations **in order** when looking for components:

```
1. ${KERNEL_DIR}/components/          built-in kernel components (this repo)
2. ${ULMK_APPS_DIR}/                  sibling directory named "ulmk_apps"
3. ${UL_CHIP_DIR}/                    board directory (current policy unchanged)
```

### Scan algorithm

For each scan root:
1. Enumerate immediate subdirectories (non-recursive at this level).
2. If a subdirectory contains a `CMakeLists.txt` with a `ul_component_register()` call, it is a component candidate.
3. Call `add_subdirectory()` on the candidate; `ul_component_register()` executes.
4. If `ENABLED OFF`, the component is registered as known but contributes nothing.
5. If `ENABLED ON`, the component's sources, include dirs, and optional linker fragment are accumulated.

### Board directory

The `boards/<board>/` directory is **not** scanned for components via the component system. Board sources are added through `board.cmake` as today. This keeps board-specific code separate from portable components.

> **Note:** A future revision may allow a board to declare companion components (e.g., a board-specific console driver). For now, board console code is either a built-in kernel component (like `board_console`) or placed directly in the board sources.

### `ULMK_APPS_DIR` resolution

```cmake
# Auto-detected if the sibling exists; overridable on command line.
set(ULMK_APPS_DIR "${CMAKE_SOURCE_DIR}/../ulmk_apps"
    CACHE PATH "Path to the ulmk_apps sibling directory")
```

---

## 5. Build Flow

```
cmake configure:
  ┌─ scan kernel/components/  → ul_component_register() calls
  ├─ scan ulmk_apps/          → ul_component_register() calls  (if exists)
  │   (board scanned separately via board.cmake, unchanged)
  ├─ validate: exactly one ROOT_THREAD component is ENABLED
  ├─ accumulate: SRCS, INCLUDE_DIRS, LINKER_FRAGMENTS from all ENABLED components
  └─ ul_generate_linker_script()  (existing mechanism, extended with component fragments)

cmake build:
  compile: kernel sources + all enabled component sources
  link:    ulipe.elf  -T generated.ld
```

### What changes vs. today

| Today | With components |
|---|---|
| `ulmk_apps/` sibling contributes a single `CMakeLists.txt` | Multiple component subdirs, each with its own descriptor |
| Root thread stub always linked | Root thread comes from the `ROOT_THREAD` component |
| No per-component include paths | Each component exposes its own `include/` |
| No per-component linker fragments | Optional `linker.ld.in` per component |

---

## 6. Linker Fragment Contract

If a component declares a `LINKER_FRAGMENT`, that `.ld.in` file is appended to the generated linker script after the kernel's own sections.

### Allowed in component linker fragments

- `SECTIONS` blocks using `UL_DOMAIN_BSS(name)` / `UL_DOMAIN_DATA(name)` macros (already defined in `include/ul/linker.h`)
- `ALIGN(UL_MPU_ALIGN)` directives before each domain boundary

### Not allowed

- Redefinition of kernel regions (`KERNEL_FLASH`, `KERNEL_RAM`, etc.)
- `MEMORY` blocks — those live in Layer 3 (chip `memory.ld`)
- Absolute address assignments

### Example fragment

```ld
/* components/board_console/linker.ld.in */
.domain_board_console (NOLOAD) :
{
    . = ALIGN(UL_MPU_ALIGN);
    _ul_domain_board_console_start = .;
    *(.domain_board_console.bss)
    *(.domain_board_console.data)
    _ul_domain_board_console_end = .;
    . = ALIGN(UL_MPU_ALIGN);
} > KERNEL_RAM
```

---

## 7. Root Thread Ownership

Exactly **one** enabled component must declare the `ROOT_THREAD` flag. The build system enforces this at configure time:

```
Error: no component declares ROOT_THREAD — image cannot boot.
Error: multiple components declare ROOT_THREAD: hello_world, other — ambiguous.
```

The `ROOT_THREAD` component is responsible for the complete system initialisation sequence inside userspace:

```c
void ul_root_thread(const ul_boot_info_t *info)
{
    /* 1. Start service threads for each enabled component. */
    board_console_start();   /* or however the component exposes its init */
    hello_world_start();

    /* 2. Hand off the spawn capability if needed. */
    /* ul_cap_grant(mc_sup_tid, UL_CAP_SPAWN); */

    ul_thread_exit();
}
```

There is no automatic component initialisation — the root thread calls component init functions explicitly, in the order it chooses. This is a deliberate policy decision: the kernel has no component registry, only threads.

---

## 8. Component Interactions — IPC Contract

Components that expose services to other components must do so via IPC endpoints. Direct function calls across component boundaries are only valid for pure library components (no server thread, no shared state). Most service components will follow this pattern:

### Service component (server side)

```c
/* Server thread entry */
static void my_service_entry(void *arg)
{
    ul_ep_t ep = ul_ep_create();
    my_service_publish_ep(ep);   /* make ep visible to clients */

    for (;;) {
        ul_msg_t  msg;
        ul_tid_t  sender;
        ul_ep_recv(ep, &msg, &sender);
        /* handle msg ... */
        ul_ep_reply(sender, &reply);
    }
}
```

### Client library (client side, compiled into caller)

```c
/* console.c — compiled into hello_world component */
void console_puts(const char *s)
{
    ul_msg_t msg = { .label = CONSOLE_MSG_PUTS, .ptr = (uintptr_t)s };
    ul_ep_call(g_console_ep, &msg, NULL);
}
```

### Endpoint discovery

How clients find server endpoints is userspace policy. Two simple options:

1. **Root thread passes ep during init** — root thread creates the server, gets back the ep, and passes it as an argument to client threads.
2. **Global volatile variable** — server publishes its ep into a shared global after creation; clients spin-poll until non-zero. Simple and sufficient for boot-time services.

The spec does not mandate either; components document their own discovery contract.

---

## 9. hello_world Component Design

**Location:** `components/hello_world/`

**Purpose:** Reference component demonstrating the full IPC-based console pattern. Acts as the `ROOT_THREAD` provider for development images.

### Files

```
components/hello_world/
├── CMakeLists.txt
├── include/
│   └── console.h           # console_puts, console_printf
├── src/
│   ├── root_thread.c       # ul_root_thread() — starts board_console + hello task
│   ├── hello_world.c       # the "hello" task: loops calling console_printf
│   └── console.c           # client lib: IPC to board_console endpoint
└── linker.ld.in            # (empty or minimal — hello_world has no private domain)
```

### CMakeLists.txt

```cmake
ul_component_register(
    NAME         hello_world
    ENABLED      ON
    SOURCES
        src/root_thread.c
        src/hello_world.c
        src/console.c
    INCLUDE_DIRS include
    REQUIRES     board_console
    ROOT_THREAD
)
```

### root_thread.c (sketch)

```c
void ul_root_thread(const ul_boot_info_t *info)
{
    ul_tid_t console_tid = board_console_start(info);
    ul_cap_grant(console_tid, UL_CAP_TIMER);

    ul_thread_attr_t attr = {
        .name       = "hello",
        .entry      = hello_world_entry,
        .priority   = 10u,
        .stack_size = 1024u,
        .privilege  = UL_PRIV_USER,
    };
    ul_tid_t hello_tid = ul_thread_create(&attr);
    ul_cap_grant(hello_tid, UL_CAP_TIMER);

    ul_thread_exit();
}
```

### console.h public API

```c
void console_puts(const char *s);
int  console_printf(const char *fmt, ...);
```

These call into `board_console` via IPC. They block until the server processes the message.

---

## 10. board_console Component Design

**Location:** `components/board_console/`

**Purpose:** Board-agnostic console service. Maps the board's physical console MMIO into its own memory domain, then exposes an IPC endpoint. Other components use `console_puts`/`console_printf` (from `hello_world/include/console.h`, or a standalone library) to write to the console without needing MMIO access themselves.

### Files

```
components/board_console/
├── CMakeLists.txt
├── include/
│   └── board_console.h     # board_console_start() init function
├── src/
│   └── board_console.c     # server thread: ul_mem_map, IPC recv loop
└── linker.ld.in            # .domain_board_console section (MMIO mapping target)
```

### CMakeLists.txt

```cmake
ul_component_register(
    NAME         board_console
    ENABLED      ON
    SOURCES      src/board_console.c
    INCLUDE_DIRS include
    LINKER_FRAGMENT linker.ld.in
)
```

### IPC message protocol

```c
/* Message labels */
#define BOARD_CONSOLE_MSG_PUTS   1u   /* ptr → null-terminated string */
#define BOARD_CONSOLE_MSG_PUTC   2u   /* data[0] → char */
```

Payload fits in the 8-byte `ul_msg_t.contents` inline field for single characters; for strings, the client passes a pointer (requires the server to have a READ grant on the caller's string buffer — or the caller maps the string into a shared region).

> **Design decision needed:** For the first version, strings can be limited to 8 bytes inline (sufficient for single-line debug messages). A `ul_mem_grant` path can be added later for longer strings.

### board_console.c (sketch)

```c
static ul_ep_t g_console_ep;  /* published for clients */

static void console_server(void *arg)
{
    void *mmio = ul_mem_map(
        (void *)BOARD_CONSOLE_VIRT_BASE,
        BOARD_CONSOLE_VIRT_SIZE,
        UL_PERM_READ | UL_PERM_WRITE,
        UL_MMAP_PERIPH);

    /* initialise low-level console (if needed) */
    board_console_hw_init(mmio);

    g_console_ep = ul_ep_create();   /* signal clients: ep is ready */

    for (;;) {
        ul_msg_t msg;
        ul_tid_t sender;
        ul_ep_recv(g_console_ep, &msg, &sender);

        switch (msg.label) {
        case BOARD_CONSOLE_MSG_PUTS:
            /* write null-terminated payload */
            board_console_puts(mmio, (const char *)msg.data);
            break;
        case BOARD_CONSOLE_MSG_PUTC:
            board_console_putc(mmio, (char)msg.data[0]);
            break;
        }
        ul_ep_reply(sender, &(ul_msg_t){0});
    }
}

ul_tid_t board_console_start(const ul_boot_info_t *info)
{
    ul_thread_attr_t attr = {
        .name       = "bcon",
        .entry      = console_server,
        .priority   = 1u,    /* high priority — console must never starve */
        .stack_size = 1024u,
        .privilege  = UL_PRIV_DRIVER,
    };
    return ul_thread_create(&attr);
}
```

---

## 11. Open Questions

The following points require your decision before implementation begins:

1. **String payload in IPC:** Should `BOARD_CONSOLE_MSG_PUTS` use the 8-byte inline field (limiting strings to 7 chars + null), or should it require the caller to `ul_mem_grant` a READ on its string buffer? The grant approach is general but adds latency and complexity for a console.

2. **Endpoint discovery:** Should `board_console` publish its endpoint via a global variable (simple, boot-only) or via a named capability table (more robust, more kernel involvement)?

3. **`board_console` as kernel component vs. board source:** Should `board_console` live in `components/board_console/` (portable, board-agnostic server with a board-specific `BOARD_CONSOLE_VIRT_BASE` define) or as a board source in `boards/qemu_tc3xx/`? A kernel component is cleaner for portability; a board source avoids the need for a new scan location.

4. **Component dependency enforcement:** `REQUIRES` is currently advisory. Should the build fail if a required component is `DISABLED`? Or should `REQUIRES` only affect link ordering?

5. **`linker.ld.in` in hello_world:** Does `hello_world` actually need a linker fragment? If it has no private memory domains (all console I/O goes through board_console), the fragment can be omitted entirely.

6. **`ul_component_register` in dev.py:** The `dev.py` build path parses board sources from `board.cmake` directly (not via CMake). Should `dev.py` also parse component `CMakeLists.txt` files for `ENABLED` components and their `SOURCES`, or should the dev.py build be migrated to invoke CMake directly (eliminating the parallel parsing)?

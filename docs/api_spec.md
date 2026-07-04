# ulmk — Public API Specification

**Version:** 1.0
**Header:** `#include <ulmk/microkernel.h>`
**Status:** Reflects the implemented API in `include/ulmk/microkernel.h`.

> **Purpose of this document:** complete reference for all syscall wrappers
> exposed to userspace components.  Consult this when writing a component or
> a board service.  For the arch-internal contract see `docs/arch_api_spec.md`.
> For the build system see `docs/build_system_spec.md`.

---

## Table of Contents

1. [Model](#1-model)
2. [Types and Constants](#2-types-and-constants)
3. [Error Codes](#3-error-codes)
4. [Privilege Levels](#4-privilege-levels)
5. [Capabilities](#5-capabilities)
6. [Thread API](#6-thread-api)
7. [IPC Endpoint API](#7-ipc-endpoint-api)
8. [Notification API](#8-notification-api)
9. [Memory API](#9-memory-api)
10. [IRQ API](#10-irq-api)
11. [Board Timer Service](#11-board-timer-service)
12. [Syscall Number Table](#12-syscall-number-table)
13. [Capability Grant API](#13-capability-grant-api)
14. [Boot Entry Point](#14-boot-entry-point)

---

## 1. Model

All public API functions are **static inline syscall wrappers** defined in
`include/ulmk/microkernel.h`.  They issue a TriCore `SYSCALL` instruction; the
kernel router in `kernel/syscall/syscall_router.c` dispatches to the handler.

Calling convention (TriCore ABI):

```
SYSCALL #N         D15 = N (syscall number / TIN)
                   D4  = arg0
                   D5  = arg1
                   D6  = arg2
                   D7  = arg3
                   D2  = return value (written by kernel before RFE)
```

Up to 4 arguments fit in registers.  When more are needed, the caller allocates
a small struct on its stack and passes a pointer as one of the four arguments
(see `ulmk_ep_reply_recv` and `ulmk_ep_recv_or_notif`).

The kernel entry path (`ulmk_arch_syscall_entry` in `arch/tricore/arch.c`) raises
`CCPN = 255` before calling the syscall router, disabling all hardware IRQs for
the duration of the syscall.

---

## 2. Types and Constants

```c
typedef int32_t   ulmk_tid_t;      /* thread ID — raw pointer to TCB */
typedef uintptr_t ulmk_ep_t;       /* IPC endpoint handle — raw pointer */
typedef uintptr_t ulmk_notif_t;    /* notification object handle — raw pointer */

#define ULMK_TID_INVALID    ((ulmk_tid_t)-1)
#define ULMK_EP_INVALID     ((ulmk_ep_t)0)
#define ULMK_NOTIF_INVALID  ((ulmk_notif_t)0)
```

Handles are raw kernel pointers.  They are valid only while the object exists;
using a freed handle is undefined behaviour.

### IPC message

```c
#define ULMK_MSG_WORDS  6

typedef struct {
    uint32_t label;               /* caller-defined message tag */
    uint32_t words[ULMK_MSG_WORDS]; /* 24 bytes of inline payload */
} ulmk_msg_t;
```

Total inline payload: 28 bytes per message (label + 6 words).  For larger
transfers use `ulmk_mem_grant()` to share a memory region.

### Thread attributes

```c
typedef struct {
    const char    *name;        /* display name (up to 15 chars) */
    void         (*entry)(void *arg);
    void          *arg;
    uint8_t        priority;    /* 0 = highest, 255 = lowest */
    size_t         stack_size;  /* bytes; allocated from user_pool */
    ulmk_privilege_t privilege;   /* ULMK_PRIV_USER or ULMK_PRIV_DRIVER */
    size_t         heap_size;   /* 0 = no per-thread heap; last for compat */
} ulmk_thread_attr_t;
```

Always declare `ulmk_thread_attr_t attr = {0}` before setting individual fields so
that `heap_size` defaults safely to zero.

### Thread heap descriptor

```c
typedef struct {
    uintptr_t base;  /* start of the heap region within the slabAO */
    size_t    size;  /* bytes; equals attr.heap_size at creation */
} ulmk_heap_info_t;
```

Returned by `ulmk_get_thread_heap()`.

### Boot information

```c
#define ULMK_BOOT_MAX_MEM_REGIONS  4

typedef struct {
    struct {
        uintptr_t base;
        size_t    size;
    } mem[ULMK_BOOT_MAX_MEM_REGIONS];
    uint32_t  mem_count;
    uintptr_t csa_pool_base;
    size_t    csa_pool_size;
} ulmk_boot_info_t;
```

Valid only for the duration of `ulmk_root_thread()`.  Copy any fields needed
beyond bootstrap before spawning child threads.

### Memory domain descriptor

```c
#define ULMK_PERM_READ   (1u << 0)
#define ULMK_PERM_WRITE  (1u << 1)
#define ULMK_PERM_EXEC   (1u << 2)
#define ULMK_PERM_USER   (1u << 3)

typedef struct {
    const char *name;
    uintptr_t   start;
    uintptr_t   end;
    uint32_t    perms;
} ulmk_domain_desc_t;
```

---

## 3. Error Codes

| Constant | Value | Meaning |
|----------|-------|---------|
| `ULMK_OK` | 0 | Success |
| `ULMK_EINVAL` | −1 | Invalid argument |
| `ULMK_ENOMEM` | −2 | Out of memory / pool exhausted |
| `ULMK_EPERM` | −3 | Permission denied (missing capability or privilege) |
| `ULMK_ENOSPC` | −4 | No space in table |
| `ULMK_EDEADLK` | −5 | Would deadlock |
| `ULMK_ESRCH` | −6 | Thread or object not found |
| `ULMK_ETIMEOUT` | −7 | Timer deadline expired before operation completed |

---

## 4. Privilege Levels

```c
typedef enum {
    ULMK_PRIV_USER   = 0,   /* PSW.IO = 0: no peripheral access */
    ULMK_PRIV_DRIVER = 1,   /* PSW.IO = 1: peripheral access, restricted syscalls */
    ULMK_PRIV_KERNEL = 2,   /* PSW.IO = 2: supervisor — kernel-internal only */
} ulmk_privilege_t;
```

The root thread starts at `ULMK_PRIV_DRIVER`.  Most board services and driver
threads run at `ULMK_PRIV_DRIVER`.  Untrusted application threads use
`ULMK_PRIV_USER`.

---

## 5. Capabilities

Capabilities are a bitmask in the TCB.  Checked by the syscall router before
privileged operations.

| Constant | Bit | Operation gated |
|----------|-----|----------------|
| `ULMK_CAP_SPAWN` | 0 | `ulmk_thread_create()` |
| `ULMK_CAP_KILL` | 1 | `ulmk_thread_kill()` |
| `ULMK_CAP_IRQ` | 2 | `ulmk_irq_bind()`, `ulmk_irq_enable()`, `ulmk_irq_disable()`, `ulmk_irq_ack()` |
| `ULMK_CAP_MAP_PERIPH` | 3 | `ulmk_mem_map()` with `ULMK_MMAP_PERIPH` |
| `ULMK_CAP_GRANT_CAP` | 4 | `ulmk_cap_grant()` |
| `ULMK_CAP_ALL` | 0xFF | All capabilities; initial value of the root thread |

---

## 6. Thread API

### `ulmk_thread_self` — get own TID

```c
ulmk_tid_t ulmk_thread_self(void);
```

Returns the TID of the calling thread.  Available at any privilege level.

---

### `ulmk_thread_yield` — cooperative yield

```c
int ulmk_thread_yield(void);
```

Yields the CPU to the next runnable thread at the same priority (FIFO within
level).  Does **not** time-slice across equal-priority threads — use explicit
blocking or periodic yields for fairness at the same level.  Returns `ULMK_OK`.

---

### `ulmk_thread_create` — spawn a thread

```c
ulmk_tid_t ulmk_thread_create(const ulmk_thread_attr_t *attr);
```

Allocates a TCB and a stack of `attr->stack_size` bytes from the user pool,
initialises the CSA chain to start at `attr->entry(attr->arg)`, and makes the
thread runnable.

**Requires:** `ULMK_CAP_SPAWN`.

Returns the new TID, or `ULMK_TID_INVALID` on failure (`ULMK_ENOMEM`, `ULMK_ENOSPC`,
`ULMK_EPERM`).

---

### `ulmk_thread_kill` — terminate a thread

```c
int ulmk_thread_kill(ulmk_tid_t tid);
```

Terminates `tid`, frees its stack and CSA chain, and removes it from the
scheduler.  If `tid` is blocked on an endpoint or notification, it is removed
from the wait list first.

**Requires:** `ULMK_CAP_KILL`.

Returns `ULMK_OK`, `ULMK_ESRCH` (TID not found), or `ULMK_EPERM`.

---

### `ulmk_thread_suspend` / `ulmk_thread_resume`

```c
int ulmk_thread_suspend(ulmk_tid_t tid);
int ulmk_thread_resume(ulmk_tid_t tid);
```

Suspend removes the thread from the ready queue without terminating it.
Resume makes it runnable again.  A suspended thread does not consume CPU.

**Requires:** `ULMK_PRIV_DRIVER`.

---

### `ulmk_thread_priority_set` / `ulmk_thread_priority_get`

```c
int ulmk_thread_priority_set(ulmk_tid_t tid, uint8_t prio);
int ulmk_thread_priority_get(ulmk_tid_t tid);
```

Dynamic priority adjustment.  Priority 0 is highest.  Changes take effect at
the next scheduler decision point.

---

### `ulmk_thread_exit` — terminate self

```c
__attribute__((noreturn)) void ulmk_thread_exit(void);
```

Terminates the calling thread.  Never returns.  The `ulmk_root_thread()` function
must call this when bootstrapping is complete.

---

## 7. IPC Endpoint API

The IPC model is synchronous call-reply.  The caller blocks until the server
replies.  Priority inheritance: the server runs at max(server\_prio,
caller\_prio) while processing a call.

### `ulmk_ep_create`

```c
ulmk_ep_t ulmk_ep_create(void);
```

Allocates an endpoint from the kernel pool.  Returns `ULMK_EP_INVALID` if the
pool is exhausted.  The endpoint is owned by the calling thread.

**Important:** create the endpoint *before* spawning the server thread.  This
ensures clients can call the endpoint immediately after `<name>_init()` returns,
without a spin-wait.

---

### `ulmk_ep_call`

```c
int ulmk_ep_call(ulmk_ep_t ep, ulmk_msg_t *msg);
```

Sends `*msg` to the endpoint and blocks until the server calls `ulmk_ep_reply()`.
The reply overwrites `*msg` in place.

Returns `ULMK_OK` or `ULMK_EINVAL` (bad endpoint).

---

### `ulmk_ep_recv`

```c
int ulmk_ep_recv(ulmk_ep_t ep, ulmk_msg_t *msg, ulmk_tid_t *sender);
```

Blocks until a message arrives on `ep`.  Fills `*msg` with the message and
`*sender` with the caller's TID.  The server must call `ulmk_ep_reply(sender, …)`
to unblock the caller.

---

### `ulmk_ep_reply`

```c
int ulmk_ep_reply(ulmk_tid_t sender, const ulmk_msg_t *reply);
```

Sends `*reply` to the blocked `sender` and makes it runnable.

---

### `ulmk_ep_reply_recv`

```c
int ulmk_ep_reply_recv(ulmk_ep_t ep, ulmk_tid_t sender,
                     const ulmk_msg_t *reply,
                     ulmk_msg_t *next, ulmk_tid_t *next_sender);
```

Atomic reply-and-receive: replies to `sender`, then immediately blocks on `ep`
for the next call.  Avoids a round-trip through the scheduler compared with
separate `ulmk_ep_reply` + `ulmk_ep_recv`.

The four output pointers are packed into a stack-allocated `ulmk_reply_recv_args_t`
to stay within the 4-register argument limit.

---

### `ulmk_ep_grant`

```c
int ulmk_ep_grant(ulmk_ep_t ep, ulmk_tid_t target);
```

Grants access to `ep` to `target`.  The target thread can then call `ulmk_ep_call`
on it.  The endpoint owner retains full access.

---

### `ulmk_ep_recv_or_notif`

```c
int ulmk_ep_recv_or_notif(ulmk_ep_t ep, ulmk_notif_t notif,
                        uint32_t mask,
                        ulmk_msg_t *msg, ulmk_tid_t *sender,
                        uint32_t *notif_bits);
```

Blocks on either an IPC call on `ep` or a notification signal on `notif` (with
bitmask `mask`), whichever arrives first.  Useful for a server that must also
react to hardware events.

Returns `ULMK_OK`.  On return, exactly one of `*sender != ULMK_TID_INVALID` (IPC
path) or `*notif_bits != 0` (notification path) is true.

---

### `ulmk_ep_destroy`

```c
int ulmk_ep_destroy(ulmk_ep_t ep);
```

Frees the endpoint.  Any threads blocked on `ep` are unblocked with
`ULMK_EINVAL`.

---

## 8. Notification API

Notifications are a lightweight one-to-many signalling primitive.  32 bits per
object; each bit is an independent flag.  Suitable for hardware IRQ delivery and
event broadcasting.

### `ulmk_notif_create`

```c
ulmk_notif_t ulmk_notif_create(void);
```

Allocates a notification object.  Returns `ULMK_NOTIF_INVALID` if the pool is
exhausted.

---

### `ulmk_notif_signal`

```c
int ulmk_notif_signal(ulmk_notif_t notif, uint32_t bits);
```

Atomically OR `bits` into the notification state.  If any thread is blocked
waiting for those bits, it is woken.

---

### `ulmk_notif_poll`

```c
uint32_t ulmk_notif_poll(ulmk_notif_t notif, uint32_t mask);
```

Returns the current set bits matching `mask` and clears them atomically.
Returns 0 if no bits matching `mask` are set.  Does not block.

---

### `ulmk_notif_wait`

```c
int ulmk_notif_wait(ulmk_notif_t notif, uint32_t mask, uint32_t *bits);
```

Blocks until at least one bit matching `mask` is set.  Clears and returns the
matching bits in `*bits`.

---

### `ulmk_notif_destroy`

```c
int ulmk_notif_destroy(ulmk_notif_t notif);
```

Frees the notification object.  Threads blocked on it are woken with
`ULMK_EINVAL`.

---

## 9. Memory API

### SlabAO per-thread heap model

Each thread may carry a private heap allocated at creation time by setting
`attr.heap_size > 0`.  The kernel allocates a contiguous *slabAO*
(`stack_size + heap_size` bytes) from `user_pool` and covers it with a single
MPU DPR.  The TCB lives in a separate allocation so userspace cannot reach
kernel metadata through its DPR.

### `ulmk_get_thread_heap`

```c
int ulmk_get_thread_heap(ulmk_heap_info_t *info);
```

Populates `*info` with the heap base and size for the calling thread.
Returns `ULMK_OK` on success, `ULMK_EPERM` if the thread has no heap
(`attr.heap_size == 0`).

---

### `ulmk_heap_extend`

```c
int ulmk_heap_extend(size_t size);
```

Allocates `size` bytes from the global `user_pool` and adds the new region as
an additional MPU DPR for the calling thread.  Requires `ULMK_PRIV_DRIVER`.
Returns `ULMK_OK`, `ULMK_ENOMEM`, `ULMK_EPERM`, or `ULMK_ENOSPC` (DPR limit
reached).

---

### `ulmk_mem_map`

```c
void *ulmk_mem_map(void *hint, size_t size, uint32_t perms, uint32_t flags);
```

Maps a memory region.  Flags:

| Flag | Meaning |
|------|---------|
| `ULMK_MMAP_ANON` | Anonymous mapping from `user_pool` |
| `ULMK_MMAP_PERIPH` | Map a peripheral MMIO region (requires `ULMK_CAP_MAP_PERIPH`) |

---

### `ulmk_mem_unmap`

```c
int ulmk_mem_unmap(void *addr, size_t size);
```

Unmaps a previously mapped region.

---

### `ulmk_mem_grant`

```c
int ulmk_mem_grant(void *addr, size_t size, ulmk_tid_t target, uint32_t perms);
```

Grants access to a memory region to `target` thread with the specified
permissions.  Used to share large buffers between components without copying.

---

## 10. IRQ API

Requires `ULMK_PRIV_DRIVER` privilege and `ULMK_CAP_IRQ` capability.

### `ulmk_irq_bind`

```c
int ulmk_irq_bind(uint8_t srpn, ulmk_notif_t notif, uint32_t bit);
```

Binds hardware interrupt `srpn` to bit `bit` in `notif`.  When the interrupt
fires, the kernel calls `ulmk_notif_signal(notif, 1u << bit)` from the ISR.

At most `ULMK_CONFIG_MAX_IRQ_BINDINGS` bindings can be active simultaneously.

---

### `ulmk_irq_enable` / `ulmk_irq_disable`

```c
int ulmk_irq_enable(uint8_t srpn);
int ulmk_irq_disable(uint8_t srpn);
```

Enable or disable the SRC for `srpn`.

---

### `ulmk_irq_ack`

```c
int ulmk_irq_ack(uint8_t srpn);
```

Clear the pending flag in the SRC for `srpn`.  Must be called after processing
a level-triggered interrupt to prevent immediate re-entry.

---

### `ulmk_irq_bind_hw`

```c
int ulmk_irq_bind_hw(uint8_t srpn, ulmk_notif_t notif, uint32_t bit,
		     uintptr_t src_reg_addr);
```

Associate a fixed hardware SRC register (TriCore: absolute address of the
`SRC_*` register) with `srpn` and deliver `bit` in `notif` when the interrupt
fires.  Used by board services (timer, UART, etc.) that own a specific SRC.

**Requires:** `ULMK_CAP_IRQ` and `ULMK_PRIV_DRIVER`.

---

## 11. Board Timer Service

The kernel has **no** system timer or sleep syscall.  Timekeeping is board
policy: a driver thread maps the SoC timer block, binds its compare-match IRQ
via `ulmk_irq_bind_hw()`, and serves sleep requests over IPC.

QEMU reference: `boards/qemu_tc3xx/board_timer.c` (STM0 + SRPN 1).

### `board_timer_start`

```c
ulmk_tid_t board_timer_start(const ulmk_boot_info_t *info);
```

Spawn the timer server thread.  Called once from `board_services_init()`.
Returns the server TID.

### `board_timer_sleep_us`

```c
void board_timer_sleep_us(uint32_t us);
```

Block the caller until approximately `us` microseconds have elapsed (IPC to
the board timer server).  Available after `board_timer_start()`.

Real boards provide their own `board_timer.c` or a weak stub in
`stub/board_timer_stub.c`.

## 12. Syscall Number Table

Defined in `include/ulmk/syscall_nr.h`.  Single source of truth for both
userspace wrappers and the kernel router.

```
 1–3   Memory (shared) (ULMK_SYS_MMAP, MUNMAP, MEM_GRANT)
 4–6   [reserved]      (former MALLOC/FREE/ALIGNED_ALLOC removed in slabAO model)
 7–8   Per-thread heap (HEAP_EXTEND, GET_THREAD_HEAP)
10–14  Scheduling      (YIELD, EXIT, [12–14 reserved — former kernel timer syscalls])
20     Thread query    (THREAD_SELF)
30–37  IPC endpoints   (EP_CREATE, CALL, RECV, REPLY, REPLY_RECV, GRANT,
                        RECV_OR_NOTIF, DESTROY)
40–44  Notifications   (NOTIF_CREATE, SIGNAL, WAIT, POLL, DESTROY)
60–64  IRQ             (IRQ_BIND, IRQ_ENABLE, IRQ_DISABLE, IRQ_ACK, IRQ_BIND_HW)
70–75  Thread mgmt     (SPAWN, KILL, SUSPEND, RESUME, SET_PRIO, GET_PRIO)
80–84  Process mgmt    (PROC_CREATE, DESTROY, ADD_REGION, GRANT_CAP, GRANT_IRQ)
```

Numbers are sparse on purpose — room for additions per group without
renumbering.

Router upper bound: `ULMK_SYS_MAX = 128`.

---

## 13. Capability Grant API

### `ulmk_cap_grant`

```c
int ulmk_cap_grant(ulmk_tid_t target, uint32_t caps);
```

Grants the capability bits in `caps` to `target`.  The caller must itself hold
`ULMK_CAP_GRANT_CAP`.  The root thread starts with `ULMK_CAP_ALL`.

Typical boot pattern:

```c
ulmk_tid_t driver = ulmk_thread_create(&driver_attr);
ulmk_cap_grant(driver, ULMK_CAP_IRQ | ULMK_CAP_MAP_PERIPH);
```

---

## 14. Boot Entry Point

```c
void ulmk_root_thread(const ulmk_boot_info_t *info);
```

User-provided strong symbol.  Called by the kernel as the first and only
userspace context.  Runs at `ULMK_PRIV_DRIVER` with `ULMK_CAP_ALL`.

Rules:
- Must never return.  Call `ulmk_thread_exit()` when bootstrapping is complete.
- Valid for the duration of `ulmk_root_thread()` only — copy `info` fields before
  spawning threads.
- Is the only thread in existence at entry; all other threads are created here
  or by threads created here.

See `docs/component_spec.md` for the component init convention and
`docs/application_development_guide.md` for a worked example.

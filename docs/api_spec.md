# ulipeMicroKernel — Public API Specification

**Version:** 1.0
**Header:** `#include <ul/microkernel.h>`
**Status:** Reflects the implemented API in `include/ul/microkernel.h`.

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
11. [Timer Primitives](#11-timer-primitives)
12. [Syscall Number Table](#12-syscall-number-table)
13. [Capability Grant API](#13-capability-grant-api)
14. [Boot Entry Point](#14-boot-entry-point)

---

## 1. Model

All public API functions are **static inline syscall wrappers** defined in
`include/ul/microkernel.h`.  They issue a TriCore `SYSCALL` instruction; the
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
(see `ul_ep_reply_recv` and `ul_ep_recv_or_notif`).

The kernel entry path (`ul_arch_syscall_entry` in `arch/tricore/arch.c`) raises
`CCPN = 255` before calling the syscall router, disabling all hardware IRQs for
the duration of the syscall.

---

## 2. Types and Constants

```c
typedef int32_t   ul_tid_t;      /* thread ID — raw pointer to TCB */
typedef uintptr_t ul_ep_t;       /* IPC endpoint handle — raw pointer */
typedef uintptr_t ul_notif_t;    /* notification object handle — raw pointer */

#define UL_TID_INVALID    ((ul_tid_t)-1)
#define UL_EP_INVALID     ((ul_ep_t)0)
#define UL_NOTIF_INVALID  ((ul_notif_t)0)
```

Handles are raw kernel pointers.  They are valid only while the object exists;
using a freed handle is undefined behaviour.

### IPC message

```c
#define UL_MSG_WORDS  6

typedef struct {
    uint32_t label;               /* caller-defined message tag */
    uint32_t words[UL_MSG_WORDS]; /* 24 bytes of inline payload */
} ul_msg_t;
```

Total inline payload: 28 bytes per message (label + 6 words).  For larger
transfers use `ul_mem_grant()` to share a memory region.

### Thread attributes

```c
typedef struct {
    const char    *name;        /* display name (up to 15 chars) */
    void         (*entry)(void *arg);
    void          *arg;
    uint8_t        priority;    /* 0 = highest, 255 = lowest */
    size_t         stack_size;  /* bytes; allocated from user_pool */
    ul_privilege_t privilege;   /* UL_PRIV_USER or UL_PRIV_DRIVER */
} ul_thread_attr_t;
```

### Boot information

```c
#define UL_BOOT_MAX_MEM_REGIONS  4

typedef struct {
    struct {
        uintptr_t base;
        size_t    size;
    } mem[UL_BOOT_MAX_MEM_REGIONS];
    uint32_t  mem_count;
    uint32_t  tick_hz;       /* scheduler tick rate (= UL_CONFIG_TICK_HZ) */
    uintptr_t csa_pool_base;
    size_t    csa_pool_size;
} ul_boot_info_t;
```

Valid only for the duration of `ul_root_thread()`.  Copy any fields needed
beyond bootstrap before spawning child threads.

### Memory domain descriptor

```c
#define UL_PERM_READ   (1u << 0)
#define UL_PERM_WRITE  (1u << 1)
#define UL_PERM_EXEC   (1u << 2)
#define UL_PERM_USER   (1u << 3)

typedef struct {
    const char *name;
    uintptr_t   start;
    uintptr_t   end;
    uint32_t    perms;
} ul_domain_desc_t;
```

---

## 3. Error Codes

| Constant | Value | Meaning |
|----------|-------|---------|
| `UL_OK` | 0 | Success |
| `UL_EINVAL` | −1 | Invalid argument |
| `UL_ENOMEM` | −2 | Out of memory / pool exhausted |
| `UL_EPERM` | −3 | Permission denied (missing capability or privilege) |
| `UL_ENOSPC` | −4 | No space in table |
| `UL_EDEADLK` | −5 | Would deadlock |
| `UL_ESRCH` | −6 | Thread or object not found |
| `UL_ETIMEOUT` | −7 | Timer deadline expired before operation completed |

---

## 4. Privilege Levels

```c
typedef enum {
    UL_PRIV_USER   = 0,   /* PSW.IO = 0: no peripheral access */
    UL_PRIV_DRIVER = 1,   /* PSW.IO = 1: peripheral access, restricted syscalls */
    UL_PRIV_KERNEL = 2,   /* PSW.IO = 2: supervisor — kernel-internal only */
} ul_privilege_t;
```

The root thread starts at `UL_PRIV_DRIVER`.  Most board services and driver
threads run at `UL_PRIV_DRIVER`.  Untrusted application threads use
`UL_PRIV_USER`.

---

## 5. Capabilities

Capabilities are a bitmask in the TCB.  Checked by the syscall router before
privileged operations.

| Constant | Bit | Operation gated |
|----------|-----|----------------|
| `UL_CAP_SPAWN` | 0 | `ul_thread_create()` |
| `UL_CAP_KILL` | 1 | `ul_thread_kill()` |
| `UL_CAP_IRQ` | 2 | `ul_irq_bind()`, `ul_irq_enable()`, `ul_irq_disable()`, `ul_irq_ack()` |
| `UL_CAP_MAP_PERIPH` | 3 | `ul_mem_map()` with `UL_MMAP_PERIPH` |
| `UL_CAP_GRANT_CAP` | 4 | `ul_cap_grant()` |
| `UL_CAP_TIMER` | 5 | `ul_timer_set_deadline()`, `ul_timer_wait()` |
| `UL_CAP_ALL` | 0xFF | All capabilities; initial value of the root thread |

---

## 6. Thread API

### `ul_thread_self` — get own TID

```c
ul_tid_t ul_thread_self(void);
```

Returns the TID of the calling thread.  Available at any privilege level.

---

### `ul_thread_yield` — cooperative yield

```c
int ul_thread_yield(void);
```

Yields the CPU to the next runnable thread of equal or higher priority.
Returns `UL_OK`.

---

### `ul_thread_create` — spawn a thread

```c
ul_tid_t ul_thread_create(const ul_thread_attr_t *attr);
```

Allocates a TCB and a stack of `attr->stack_size` bytes from the user pool,
initialises the CSA chain to start at `attr->entry(attr->arg)`, and makes the
thread runnable.

**Requires:** `UL_CAP_SPAWN`.

Returns the new TID, or `UL_TID_INVALID` on failure (`UL_ENOMEM`, `UL_ENOSPC`,
`UL_EPERM`).

---

### `ul_thread_kill` — terminate a thread

```c
int ul_thread_kill(ul_tid_t tid);
```

Terminates `tid`, frees its stack and CSA chain, and removes it from the
scheduler.  If `tid` is blocked on an endpoint or notification, it is removed
from the wait list first.

**Requires:** `UL_CAP_KILL`.

Returns `UL_OK`, `UL_ESRCH` (TID not found), or `UL_EPERM`.

---

### `ul_thread_suspend` / `ul_thread_resume`

```c
int ul_thread_suspend(ul_tid_t tid);
int ul_thread_resume(ul_tid_t tid);
```

Suspend removes the thread from the ready queue without terminating it.
Resume makes it runnable again.  A suspended thread does not consume CPU.

**Requires:** `UL_PRIV_DRIVER`.

---

### `ul_thread_priority_set` / `ul_thread_priority_get`

```c
int ul_thread_priority_set(ul_tid_t tid, uint8_t prio);
int ul_thread_priority_get(ul_tid_t tid);
```

Dynamic priority adjustment.  Priority 0 is highest.  Changes take effect at
the next scheduler decision point.

---

### `ul_thread_exit` — terminate self

```c
__attribute__((noreturn)) void ul_thread_exit(void);
```

Terminates the calling thread.  Never returns.  The `ul_root_thread()` function
must call this when bootstrapping is complete.

---

## 7. IPC Endpoint API

The IPC model is synchronous call-reply.  The caller blocks until the server
replies.  Priority inheritance: the server runs at max(server\_prio,
caller\_prio) while processing a call.

### `ul_ep_create`

```c
ul_ep_t ul_ep_create(void);
```

Allocates an endpoint from the kernel pool.  Returns `UL_EP_INVALID` if the
pool is exhausted.  The endpoint is owned by the calling thread.

**Important:** create the endpoint *before* spawning the server thread.  This
ensures clients can call the endpoint immediately after `<name>_init()` returns,
without a spin-wait.

---

### `ul_ep_call`

```c
int ul_ep_call(ul_ep_t ep, ul_msg_t *msg);
```

Sends `*msg` to the endpoint and blocks until the server calls `ul_ep_reply()`.
The reply overwrites `*msg` in place.

Returns `UL_OK` or `UL_EINVAL` (bad endpoint).

---

### `ul_ep_recv`

```c
int ul_ep_recv(ul_ep_t ep, ul_msg_t *msg, ul_tid_t *sender);
```

Blocks until a message arrives on `ep`.  Fills `*msg` with the message and
`*sender` with the caller's TID.  The server must call `ul_ep_reply(sender, …)`
to unblock the caller.

---

### `ul_ep_reply`

```c
int ul_ep_reply(ul_tid_t sender, const ul_msg_t *reply);
```

Sends `*reply` to the blocked `sender` and makes it runnable.

---

### `ul_ep_reply_recv`

```c
int ul_ep_reply_recv(ul_ep_t ep, ul_tid_t sender,
                     const ul_msg_t *reply,
                     ul_msg_t *next, ul_tid_t *next_sender);
```

Atomic reply-and-receive: replies to `sender`, then immediately blocks on `ep`
for the next call.  Avoids a round-trip through the scheduler compared with
separate `ul_ep_reply` + `ul_ep_recv`.

The four output pointers are packed into a stack-allocated `ul_reply_recv_args_t`
to stay within the 4-register argument limit.

---

### `ul_ep_grant`

```c
int ul_ep_grant(ul_ep_t ep, ul_tid_t target);
```

Grants access to `ep` to `target`.  The target thread can then call `ul_ep_call`
on it.  The endpoint owner retains full access.

---

### `ul_ep_recv_or_notif`

```c
int ul_ep_recv_or_notif(ul_ep_t ep, ul_notif_t notif,
                        uint32_t mask,
                        ul_msg_t *msg, ul_tid_t *sender,
                        uint32_t *notif_bits);
```

Blocks on either an IPC call on `ep` or a notification signal on `notif` (with
bitmask `mask`), whichever arrives first.  Useful for a server that must also
react to hardware events.

Returns `UL_OK`.  On return, exactly one of `*sender != UL_TID_INVALID` (IPC
path) or `*notif_bits != 0` (notification path) is true.

---

### `ul_ep_destroy`

```c
int ul_ep_destroy(ul_ep_t ep);
```

Frees the endpoint.  Any threads blocked on `ep` are unblocked with
`UL_EINVAL`.

---

## 8. Notification API

Notifications are a lightweight one-to-many signalling primitive.  32 bits per
object; each bit is an independent flag.  Suitable for hardware IRQ delivery and
event broadcasting.

### `ul_notif_create`

```c
ul_notif_t ul_notif_create(void);
```

Allocates a notification object.  Returns `UL_NOTIF_INVALID` if the pool is
exhausted.

---

### `ul_notif_signal`

```c
int ul_notif_signal(ul_notif_t notif, uint32_t bits);
```

Atomically OR `bits` into the notification state.  If any thread is blocked
waiting for those bits, it is woken.

---

### `ul_notif_poll`

```c
uint32_t ul_notif_poll(ul_notif_t notif, uint32_t mask);
```

Returns the current set bits matching `mask` and clears them atomically.
Returns 0 if no bits matching `mask` are set.  Does not block.

---

### `ul_notif_wait`

```c
int ul_notif_wait(ul_notif_t notif, uint32_t mask, uint32_t *bits);
```

Blocks until at least one bit matching `mask` is set.  Clears and returns the
matching bits in `*bits`.

---

### `ul_notif_destroy`

```c
int ul_notif_destroy(ul_notif_t notif);
```

Frees the notification object.  Threads blocked on it are woken with
`UL_EINVAL`.

---

## 9. Memory API

### `ul_malloc` / `ul_free`

```c
void *ul_malloc(size_t size);
void  ul_free(void *ptr);
```

TLSF allocator backed by the `user_pool` linker region.  Thread-safe.

---

### `ul_aligned_alloc`

```c
void *ul_aligned_alloc(size_t align, size_t size);
```

Like `ul_malloc` but guarantees `align`-byte alignment.  `align` must be a
power of two.

---

### `ul_mem_map`

```c
void *ul_mem_map(void *hint, size_t size, uint32_t perms, uint32_t flags);
```

Maps a memory region.  Flags:

| Flag | Meaning |
|------|---------|
| `UL_MMAP_ANON` | Anonymous mapping from `user_pool` |
| `UL_MMAP_PERIPH` | Map a peripheral MMIO region (requires `UL_CAP_MAP_PERIPH`) |

---

### `ul_mem_unmap`

```c
int ul_mem_unmap(void *addr, size_t size);
```

Unmaps a previously mapped region.

---

### `ul_mem_grant`

```c
int ul_mem_grant(void *addr, size_t size, ul_tid_t target, uint32_t perms);
```

Grants access to a memory region to `target` thread with the specified
permissions.  Used to share large buffers between components without copying.

---

## 10. IRQ API

Requires `UL_PRIV_DRIVER` privilege and `UL_CAP_IRQ` capability.

### `ul_irq_bind`

```c
int ul_irq_bind(uint8_t srpn, ul_notif_t notif, uint32_t bit);
```

Binds hardware interrupt `srpn` to bit `bit` in `notif`.  When the interrupt
fires, the kernel calls `ul_notif_signal(notif, 1u << bit)` from the ISR.

At most `UL_CONFIG_MAX_IRQ_BINDINGS` bindings can be active simultaneously.

---

### `ul_irq_enable` / `ul_irq_disable`

```c
int ul_irq_enable(uint8_t srpn);
int ul_irq_disable(uint8_t srpn);
```

Enable or disable the SRC for `srpn`.

---

### `ul_irq_ack`

```c
int ul_irq_ack(uint8_t srpn);
```

Clear the pending flag in the SRC for `srpn`.  Must be called after processing
a level-triggered interrupt to prevent immediate re-entry.

---

## 11. Timer Primitives

Requires `UL_CAP_TIMER`.  These are **low-level kernel timer operations** for
the timer server.  Application-level sleep should be implemented by sending an
IPC request to a timer server component.

### `ul_timer_set_deadline`

```c
int ul_timer_set_deadline(uint64_t deadline_us);
```

Programs the hardware timer to expire `deadline_us` microseconds from now.
Only one deadline can be active at a time; a new call overwrites the previous
one.

The 64-bit value is split across two 32-bit register arguments following the
TriCore ABI (`D4 = low`, `D5 = high`).

---

### `ul_timer_wait`

```c
int ul_timer_wait(void);
```

Blocks the calling thread until the programmed deadline expires.  Exactly one
thread may call this at a time.

---

## 12. Syscall Number Table

Defined in `include/ul/syscall_nr.h`.  Single source of truth for both
userspace wrappers and the kernel router.

```
 1–6   Memory          (UL_SYS_MMAP, MUNMAP, MEM_GRANT, MALLOC, FREE, ALIGNED_ALLOC)
10–14  Scheduling      (YIELD, EXIT, [12 reserved], TIMER_SETDEADLINE, TIMER_WAIT)
20     Thread query    (THREAD_SELF)
30–37  IPC endpoints   (EP_CREATE, CALL, RECV, REPLY, REPLY_RECV, GRANT,
                        RECV_OR_NOTIF, DESTROY)
40–44  Notifications   (NOTIF_CREATE, SIGNAL, WAIT, POLL, DESTROY)
60–63  IRQ             (IRQ_BIND, IRQ_ENABLE, IRQ_DISABLE, IRQ_ACK)
70–75  Thread mgmt     (SPAWN, KILL, SUSPEND, RESUME, SET_PRIO, GET_PRIO)
80–84  Process mgmt    (PROC_CREATE, DESTROY, ADD_REGION, GRANT_CAP, GRANT_IRQ)
```

Numbers are sparse on purpose — room for additions per group without
renumbering.

Router upper bound: `UL_SYS_MAX = 128`.

---

## 13. Capability Grant API

### `ul_cap_grant`

```c
int ul_cap_grant(ul_tid_t target, uint32_t caps);
```

Grants the capability bits in `caps` to `target`.  The caller must itself hold
`UL_CAP_GRANT_CAP`.  The root thread starts with `UL_CAP_ALL`.

Typical boot pattern:

```c
ul_tid_t timer_srv = ul_thread_create(&timer_attr);
ul_cap_grant(timer_srv, UL_CAP_TIMER);

ul_tid_t driver = ul_thread_create(&driver_attr);
ul_cap_grant(driver, UL_CAP_IRQ | UL_CAP_MAP_PERIPH);
```

---

## 14. Boot Entry Point

```c
void ul_root_thread(const ul_boot_info_t *info);
```

User-provided strong symbol.  Called by the kernel as the first and only
userspace context.  Runs at `UL_PRIV_DRIVER` with `UL_CAP_ALL`.

Rules:
- Must never return.  Call `ul_thread_exit()` when bootstrapping is complete.
- Valid for the duration of `ul_root_thread()` only — copy `info` fields before
  spawning threads.
- Is the only thread in existence at entry; all other threads are created here
  or by threads created here.

See `docs/component_spec.md` for the component init convention and
`docs/application_development_guide.md` for a worked example.

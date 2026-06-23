# ulipeMicroKernel — Public API Specification

**Version:** 0.2 (draft)
**Target:** TriCore TC1.6.1 / AURIX TC2xx
**Header:** `#include <sys/ulipe_microkernel.h>`

---

## Table of Contents

1. [Design Philosophy](#1-design-philosophy)
2. [Core Concepts](#2-core-concepts)
3. [Types and Handles](#3-types-and-handles)
4. [Error Codes](#4-error-codes)
5. [Boot Model — Root Thread](#5-boot-model--root-thread)
6. [Thread API](#6-thread-api)
7. [IPC Endpoint API](#7-ipc-endpoint-api)
8. [Notification API](#8-notification-api)
9. [Memory API](#9-memory-api)
10. [IRQ API](#10-irq-api)
11. [Privilege Model](#11-privilege-model)
12. [Syscall Numbers](#12-syscall-numbers)
13. [Userspace Library — libul](#13-userspace-library--libul)
14. [Usage Examples](#14-usage-examples)

---

## 1. Design Philosophy

The kernel exposes the minimum set of primitives to enable safe, isolated execution of drivers and applications. Everything else — naming services, lifecycle policies, resource accounting — lives in userspace.

**What the kernel does:**
- Thread scheduling (priority-based, preemptive)
- Synchronous IPC via endpoints (seL4-inspired)
- Asynchronous notification objects
- Memory protection (TriCore DPR/CPR via PRS switching)
- IRQ routing to userspace notification objects

**What the kernel does NOT do:**
- Dynamic driver loading
- File systems, network stacks
- Thread lifecycle policy (delegated to the MC Supervisor)

**Privilege tiers (TriCore PSW.IO):**

| IO Level | Name | Users |
|----------|------|-------|
| 2 | Supervisor | Kernel only |
| 1 | Driver | Drivers; may access peripherals, restricted syscalls |
| 0 | User | Applications; no peripheral access |

---

## 2. Core Concepts

### 2.1 Threads and Processes

A **thread** is the unit of execution. Every thread has:
- An 8-bit priority (0 = highest, 255 = lowest)
- A privilege level (driver or user)
- An isolated address space (code + private data + stack)

A **process** is the unit of isolation: a container of threads that share an address space and a capability set. Drivers live in their own process; apps in theirs.

Thread creation is mediated by the **MC Supervisor** (a privileged userspace server). Apps send it an IPC request; the MC Supervisor calls `SYS_THREAD_SPAWN`.

### 2.2 IPC — Endpoint Model

IPC is **synchronous** and **capability-based**:

```
CALL:  caller blocks until server replies
RECV:  server blocks waiting for messages
REPLY: server unblocks caller and delivers reply
```

An **endpoint** (`ul_ep_t`) is a kernel object representing a communication channel. To send to an endpoint, the caller must hold a capability token for it.

The IPC message (`ul_msg_t`) carries:
- `label`: 32-bit message type / opcode
- `words[6]`: 6 × 32-bit payload (192 bits inline)

For payloads larger than 192 bits, use a shared memory grant (see §9).

### 2.3 Notifications

A **notification** (`ul_notif_t`) is a 32-bit bitmask object. Threads can:
- `signal` individual bits from ISRs or other threads (non-blocking)
- `wait` for a mask of bits (blocking)
- `poll` without blocking

Notifications are the primary mechanism for IRQ delivery to userspace.

### 2.4 Memory Model

There is no virtual address translation. Addresses are physical. The MPU enforces isolation by configuring TriCore Data/Code Protection Ranges (DPR/CPR) per thread.

Memory regions:
- **Anonymous**: kernel allocates from the physical pool (buddy allocator)
- **Peripheral**: maps a specific physical peripheral address; requires capability
- **Grant**: shares a region between two address spaces (read-only or read-write)

---

## 3. Types and Handles

```c
/* Opaque handles — never dereference directly */
typedef int32_t  ul_tid_t;     /* thread ID; -1 = invalid */
typedef int32_t  ul_ep_t;      /* endpoint handle; -1 = invalid */
typedef int32_t  ul_notif_t;   /* notification handle; -1 = invalid */

#define UL_TID_INVALID    ((ul_tid_t)-1)
#define UL_EP_INVALID     ((ul_ep_t)-1)
#define UL_NOTIF_INVALID  ((ul_notif_t)-1)

/* Privilege levels (maps directly to TriCore PSW.IO field) */
typedef enum {
	UL_PRIV_USER   = 0,   /* PSW.IO = 0 — no peripheral access */
	UL_PRIV_DRIVER = 1,   /* PSW.IO = 1 — peripheral access, restricted syscalls */
} ul_privilege_t;

/* Memory permission flags */
#define UL_PERM_READ   (1u << 0)
#define UL_PERM_WRITE  (1u << 1)
#define UL_PERM_EXEC   (1u << 2)

/* Memory mapping flags */
#define UL_MMAP_ANON   (1u << 0)  /* allocate from physical pool */
#define UL_MMAP_PERIPH (1u << 1)  /* map a peripheral (needs capability) */
#define UL_MMAP_SHARED (1u << 2)  /* shared region (grant destination) */

/* IPC message — 7 words total (label + 6 payload) */
#define UL_MSG_WORDS   6

typedef struct {
	uint32_t label;
	uint32_t words[UL_MSG_WORDS];
} ul_msg_t;

/* Thread creation attributes */
typedef struct {
	const char    *name;          /* up to 15 chars, NUL-terminated */
	void         (*entry)(void *);
	void          *arg;
	uint8_t        priority;      /* 0 = highest */
	size_t         stack_size;    /* bytes; kernel rounds up to 8-byte alignment */
	ul_privilege_t privilege;
} ul_thread_attr_t;

/* Boot information passed to ul_root_thread() by the kernel */
#define UL_BOOT_MAX_MEM_REGIONS  4

typedef struct {
	struct {
		uintptr_t base;
		size_t    size;
	} mem[UL_BOOT_MAX_MEM_REGIONS];   /* available physical memory regions */
	uint32_t  mem_count;
	uint32_t  tick_hz;                /* scheduler tick rate configured by arch */
	uintptr_t csa_pool_base;
	size_t    csa_pool_size;
} ul_boot_info_t;
```

---

## 4. Error Codes

All syscall wrappers return a negative value on failure.

| Code | Meaning |
|------|---------|
| `0` | Success |
| `-UL_EPERM` | Operation not permitted (privilege check failed) |
| `-UL_EINVAL` | Invalid argument |
| `-UL_ENOMEM` | Out of physical memory |
| `-UL_ENOSPC` | Resource table full (endpoints, threads, regions) |
| `-UL_ENOENT` | Handle not found or not owned by caller |
| `-UL_EDEADLK` | IPC would deadlock |
| `-UL_ETIMEOUT` | Operation timed out |

```c
#define UL_EPERM    1
#define UL_EINVAL   2
#define UL_ENOMEM   3
#define UL_ENOSPC   4
#define UL_ENOENT   5
#define UL_EDEADLK  6
#define UL_ETIMEOUT 7
```

---

## 5. Boot Model — Root Thread

The kernel is launched by the architecture's `startup.S`, not by user code.
The user never calls `ul_kernel_init()` or `ul_kernel_start()` — those do not
exist in the public API.

### Boot Sequence

```
startup.S (_start)
  └─► ul_arch_init()          [internal]  stack, CSA, BTV/BIV, tick timer
        └─► ul_kernel_main()  [internal]  phys allocator, MPU, scheduler
              └─► root_thread is created and the scheduler starts
```

### Root Thread Entry Point

The user defines exactly one function:

```c
/*
 * ul_root_thread - first userspace context; called once at boot
 *
 * @info: pointer to boot information; valid only for the duration of this
 *        call.  Copy any fields needed after bootstrap completes.
 *
 * Runs at privilege UL_PRIV_DRIVER.  Must not return; call ul_thread_exit()
 * when bootstrap is complete.
 */
void ul_root_thread(const ul_boot_info_t *info);
```

The kernel resolves `ul_root_thread` as a weak extern symbol; the linker
ensures user code provides it.

### Root Thread Capabilities

At entry, the root thread implicitly holds:
- The **spawn capability**: right to call `ul_thread_create()`.
- The **grant capability**: right to delegate capabilities to other threads.
- Access to all physical memory described in `ul_boot_info_t`.

These can be transferred to the MC Supervisor once it is running.

### Minimal Root Thread

```c
#include <sys/ulipe_microkernel.h>

extern void mc_supervisor_entry(void *);

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr = {
		.name       = "mc_sup",
		.entry      = mc_supervisor_entry,
		.arg        = (void *)info,
		.priority   = 0,
		.stack_size = 4096,
		.privilege  = UL_PRIV_DRIVER,
	};

	ul_thread_create(&attr);
	ul_thread_exit();
}
```

---

## 6. Thread API

### 6.1 Self

```c
/*
 * ul_thread_self - return the calling thread's ID
 */
ul_tid_t ul_thread_self(void);
```

### 6.2 Yield

```c
/*
 * ul_thread_yield - voluntarily relinquish the CPU
 *
 * The calling thread stays READY; another thread of equal or higher
 * priority runs next.
 */
void ul_thread_yield(void);
```

### 6.3 Exit

```c
/*
 * ul_thread_exit - terminate the calling thread
 *
 * Does not return.  The thread's stack is freed; its process remains alive
 * if other threads exist within it.
 */
void ul_thread_exit(void);
```

### 6.4 Create (requires spawn capability)

```c
/*
 * ul_thread_create - spawn a new thread
 *
 * @attr: thread attributes; all fields must be filled in.
 *
 * Requires the spawn capability.  Initially only the root thread and
 * threads it delegates to (via ul_cap_grant) hold this capability.
 * Callers without the spawn capability receive -UL_EPERM.
 *
 * Returns the new thread's ID on success, or a negative error code.
 */
ul_tid_t ul_thread_create(const ul_thread_attr_t *attr);
```

### 6.5 Kill (requires spawn capability)

```c
/*
 * ul_thread_kill - forcibly terminate a thread
 *
 * All resources owned exclusively by @tid are released.
 * Endpoints and notifications it created remain valid until
 * their last capability holder releases them.
 *
 * Returns 0 or negative error code.
 */
int ul_thread_kill(ul_tid_t tid);
```

### 6.6 Suspend / Resume (requires spawn capability)

```c
/*
 * ul_thread_suspend - prevent a thread from being scheduled
 * ul_thread_resume  - make a suspended thread runnable again
 *
 * Suspensions are counted: a thread suspended N times requires N resumes.
 *
 * Returns 0 or negative error code.
 */
int ul_thread_suspend(ul_tid_t tid);
int ul_thread_resume(ul_tid_t tid);
```

### 6.7 Priority

```c
/*
 * ul_thread_priority_set - change a thread's scheduling priority
 * ul_thread_priority_get - query a thread's current priority
 *
 * Drivers (IO=1) may adjust any thread they created.
 * Apps (IO=0) may only adjust themselves.
 *
 * Returns 0 / priority value, or negative on error.
 */
int ul_thread_priority_set(ul_tid_t tid, uint8_t prio);
int ul_thread_priority_get(ul_tid_t tid);
```

---

## 7. IPC Endpoint API

### 7.1 Create

```c
/*
 * ul_ep_create - allocate an IPC endpoint owned by the calling thread
 *
 * Returns endpoint handle or negative error code.
 * The endpoint persists until the owning process is destroyed.
 */
ul_ep_t ul_ep_create(void);
```

### 7.2 CALL — blocking send + wait for reply

```c
/*
 * ul_ep_call - send a message and block until the server replies
 *
 * @ep:   destination endpoint (caller must hold the capability)
 * @msg:  message to send; on return, overwritten with the reply
 *
 * Returns 0 on successful reply, or negative error code.
 *
 * Note: caller's priority is donated to the server for the duration
 * of the call (priority inheritance).
 */
int ul_ep_call(ul_ep_t ep, ul_msg_t *msg);
```

### 7.3 RECV — blocking receive

```c
/*
 * ul_ep_recv - wait for a message on an endpoint the thread owns
 *
 * @ep:      endpoint to listen on
 * @msg:     filled with the incoming message
 * @sender:  filled with the sender's TID (for ul_ep_reply)
 *
 * Returns 0 or negative error code.
 */
int ul_ep_recv(ul_ep_t ep, ul_msg_t *msg, ul_tid_t *sender);
```

### 7.4 REPLY

```c
/*
 * ul_ep_reply - unblock a thread that called ul_ep_call()
 *
 * @sender: TID received from ul_ep_recv()
 * @reply:  reply message delivered to the caller
 *
 * Returns 0 or negative error code.
 */
int ul_ep_reply(ul_tid_t sender, const ul_msg_t *reply);
```

### 7.5 REPLY + RECV (server fast path)

```c
/*
 * ul_ep_reply_recv - atomically reply and wait for the next message
 *
 * Avoids an extra context switch in the common server loop:
 *   reply to previous caller → immediately block on next message.
 *
 * @ep:      endpoint to receive on after replying
 * @sender:  TID of the previous caller (to reply to)
 * @reply:   reply for the previous caller
 * @msg:     filled with the new incoming message
 * @next:    filled with the new sender's TID
 */
int ul_ep_reply_recv(ul_ep_t ep, ul_tid_t sender, const ul_msg_t *reply,
		     ul_msg_t *msg, ul_tid_t *next);
```

### 7.6 Capability transfer

```c
/*
 * ul_ep_grant - give another thread the capability to use an endpoint
 *
 * @ep:     endpoint to grant access to
 * @target: TID of the recipient thread
 *
 * Returns 0 or negative error code.
 */
int ul_ep_grant(ul_ep_t ep, ul_tid_t target);
```

---

## 8. Notification API

### 8.1 Create

```c
/*
 * ul_notif_create - allocate a notification object
 *
 * Returns handle or negative error code.
 */
ul_notif_t ul_notif_create(void);
```

### 8.2 Signal

```c
/*
 * ul_notif_signal - set bits in a notification object (non-blocking)
 *
 * Safe to call from ISR context.
 *
 * @notif: notification handle
 * @bits:  bitmask to OR into the notification word
 */
void ul_notif_signal(ul_notif_t notif, uint32_t bits);
```

### 8.3 Wait

```c
/*
 * ul_notif_wait - block until at least one bit in @mask is set
 *
 * On return, the matched bits are cleared atomically.
 *
 * @notif: notification handle
 * @mask:  bits to wait for (any bit in mask triggers wakeup)
 *
 * Returns the bits that triggered the wakeup.
 */
uint32_t ul_notif_wait(ul_notif_t notif, uint32_t mask);
```

### 8.4 Poll

```c
/*
 * ul_notif_poll - non-blocking check; clears and returns matched bits
 *
 * Returns the bits that were set and matched @mask; 0 if none were set.
 */
uint32_t ul_notif_poll(ul_notif_t notif, uint32_t mask);
```

### 8.5 RECV or NOTIFY (combined wait)

```c
/*
 * ul_ep_recv_or_notif - wait for an IPC message or a notification
 *
 * Returns when either an IPC message arrives on @ep or bits matching
 * @mask are set in @notif.  Useful for server threads that also handle IRQs.
 *
 * @ep:      endpoint to receive from
 * @notif:   notification to watch
 * @mask:    bits to watch in the notification
 * @msg:     filled with the IPC message (if woken by IPC)
 * @sender:  filled with sender TID (if woken by IPC); UL_TID_INVALID if notif
 * @bits:    filled with matched notification bits (0 if woken by IPC)
 */
int ul_ep_recv_or_notif(ul_ep_t ep, ul_notif_t notif, uint32_t mask,
			ul_msg_t *msg, ul_tid_t *sender, uint32_t *bits);
```

---

## 9. Memory API

### 9.1 Map anonymous memory

```c
/*
 * ul_mem_map - map a region into the calling thread's address space
 *
 * @hint:  preferred physical address; NULL lets the kernel choose.
 *         For UL_MMAP_PERIPH, @hint is the exact peripheral base address.
 * @size:  requested size in bytes; rounded up to 8-byte alignment (TriCore)
 * @perms: UL_PERM_READ | UL_PERM_WRITE | UL_PERM_EXEC (W^X enforced)
 * @flags: UL_MMAP_ANON | UL_MMAP_PERIPH
 *
 * Returns physical address of the mapped region, or NULL on failure.
 *
 * For UL_MMAP_PERIPH: caller must hold a peripheral capability
 * (granted by the MC Supervisor at process creation time).
 */
void *ul_mem_map(void *hint, size_t size, uint32_t perms, uint32_t flags);
```

### 9.2 Unmap

```c
/*
 * ul_mem_unmap - release a previously mapped region
 *
 * Anonymous regions are returned to the physical pool.
 * Peripheral mappings only remove the MPU entry (the peripheral remains).
 *
 * Returns 0 or negative error code.
 */
int ul_mem_unmap(void *addr, size_t size);
```

### 9.3 Grant — share region with another thread

```c
/*
 * ul_mem_grant - share a mapped region with another thread's address space
 *
 * The source thread must own the region.  The grant can be read-only even
 * if the source has write access (perms_override masks the source perms).
 *
 * @addr:          physical address of the region to share
 * @size:          size in bytes
 * @target:        TID of the recipient thread
 * @perms_override: permissions for the recipient (cannot exceed source perms)
 *
 * Returns 0 or negative error code.
 */
int ul_mem_grant(void *addr, size_t size, ul_tid_t target,
		 uint32_t perms_override);
```

---

## 10. IRQ API

IRQs are delivered to userspace via notification objects. A driver:
1. Creates a notification object.
2. Binds an IRQ number to a bit in that notification.
3. Enables the IRQ.
4. Calls `ul_notif_wait()` in its service loop.
5. After handling, calls `ul_irq_ack()` to allow further delivery.

### 10.1 Bind

```c
/*
 * ul_irq_bind - route an IRQ to a notification bit (privileged: IO >= 1)
 *
 * When the IRQ fires, the kernel sets @bit in @notif and wakes any thread
 * waiting on it.  The ISR is NOT executed in userspace directly.
 *
 * @irq_num: hardware IRQ number (TriCore SRC priority number, 1–255)
 * @notif:   notification handle to signal
 * @bit:     which bit in the notification to set (0–31)
 *
 * Returns 0 or negative error code.
 */
int ul_irq_bind(int irq_num, ul_notif_t notif, uint32_t bit);
```

### 10.2 Enable / Disable

```c
/*
 * ul_irq_enable  - enable delivery of an IRQ (privileged: IO >= 1)
 * ul_irq_disable - disable delivery without unbinding (privileged: IO >= 1)
 *
 * Returns 0 or negative error code.
 */
int ul_irq_enable(int irq_num);
int ul_irq_disable(int irq_num);
```

### 10.3 Acknowledge

```c
/*
 * ul_irq_ack - acknowledge handling and re-arm the IRQ source
 *
 * Must be called after processing the notification, before ul_notif_wait()
 * can receive the same IRQ again.
 *
 * Returns 0 or negative error code.
 */
int ul_irq_ack(int irq_num);
```

---

## 11. Privilege Model

| Syscall group | IO=0 (User) | IO=1 (Driver) |
|---------------|-------------|---------------|
| `ul_thread_self`, `ul_thread_yield`, `ul_thread_exit` | ✓ | ✓ |
| `ul_thread_priority_set` (self only) | ✓ | ✓ |
| `ul_ep_*`, `ul_notif_*` | ✓ | ✓ |
| `ul_mem_map` (anonymous) | ✓ | ✓ |
| `ul_mem_map` (peripheral) | ✗ | ✓ (with cap) |
| `ul_irq_bind`, `ul_irq_enable`, `ul_irq_ack` | ✗ | ✓ |
| `ul_thread_create`, `ul_thread_suspend/resume` | ✗ | ✓ |
| `ul_thread_priority_set` (other threads) | ✗ | ✓ |

---

## 12. Syscall Numbers

Issued via TriCore `SYSCALL #N` instruction (trap class 6, TIN = N).
Arguments in D4–D7 (data) or A4–A7 (pointers); return value in D2.

| Number | Name | Restriction |
|--------|------|-------------|
| 1 | `SYS_MMAP` | — |
| 2 | `SYS_MUNMAP` | — |
| 3 | `SYS_MEM_GRANT` | — |
| 10 | `SYS_YIELD` | — |
| 11 | `SYS_EXIT` | — |
| 20 | `SYS_THREAD_SELF` | — |
| 30 | `SYS_EP_CREATE` | — |
| 31 | `SYS_EP_CALL` | — |
| 32 | `SYS_EP_RECV` | — |
| 33 | `SYS_EP_REPLY` | — |
| 34 | `SYS_EP_REPLY_RECV` | — |
| 35 | `SYS_EP_GRANT` | — |
| 36 | `SYS_EP_RECV_OR_NOTIF` | — |
| 40 | `SYS_NOTIF_CREATE` | — |
| 41 | `SYS_NOTIF_SIGNAL` | — |
| 42 | `SYS_NOTIF_WAIT` | — |
| 43 | `SYS_NOTIF_POLL` | — |
| 60 | `SYS_IRQ_BIND` | IO >= 1 |
| 61 | `SYS_IRQ_ENABLE` | IO >= 1 |
| 62 | `SYS_IRQ_DISABLE` | IO >= 1 |
| 63 | `SYS_IRQ_ACK` | IO >= 1 |
| 70 | `SYS_THREAD_SPAWN` | IO >= 1 |
| 71 | `SYS_THREAD_KILL` | IO >= 1 |
| 72 | `SYS_THREAD_SUSPEND` | IO >= 1 |
| 73 | `SYS_THREAD_RESUME` | IO >= 1 |
| 74 | `SYS_THREAD_SET_PRIO` | IO >= 1 |
| 80 | `SYS_PROC_CREATE` | IO >= 1 |
| 81 | `SYS_PROC_DESTROY` | IO >= 1 |
| 82 | `SYS_PROC_ADD_REGION` | IO >= 1 |
| 83 | `SYS_PROC_GRANT_CAP` | IO >= 1 |
| 84 | `SYS_PROC_GRANT_IRQ` | IO >= 1 |

---

## 13. Userspace Library — libul

The kernel provides only mechanisms (IPC, notifications, memory, IRQ routing).
Higher-level RTOS primitives are implemented as a thin userspace library
(`libul`) on top of those mechanisms.  No additional syscalls are needed.

`#include <ul/libul.h>`

---

### 13.1 Mutex

A mutex is a notification with one token bit.  Initialised as *signalled*
(unlocked).  `ul_mutex_lock()` consumes the bit (blocks if already consumed);
`ul_mutex_unlock()` signals it back.

```c
typedef struct {
	ul_notif_t notif;
	uint32_t   bit;
} ul_mutex_t;

/*
 * ul_mutex_init   - allocate notification and set token bit
 * ul_mutex_lock   - acquire; blocks if already held
 * ul_mutex_unlock - release
 */
int      ul_mutex_init(ul_mutex_t *m);
void     ul_mutex_lock(ul_mutex_t *m);
void     ul_mutex_unlock(ul_mutex_t *m);
```

**Implementation note:** each mutex consumes one notification handle and one
bit of its 32-bit word.  Up to 32 mutexes can share a single notification
object if the caller manages bit assignment.

---

### 13.2 Semaphore

A counting semaphore backed by N bits in a notification word (max 32 permits).

```c
typedef struct {
	ul_notif_t notif;
	uint32_t   all_bits;   /* one bit per permit */
} ul_sem_t;

/*
 * ul_sem_init - initialise with @count permits (≤ 32)
 * ul_sem_wait - acquire one permit; blocks if none available
 * ul_sem_post - release one permit
 */
int  ul_sem_init(ul_sem_t *s, uint8_t count);
void ul_sem_wait(ul_sem_t *s);
void ul_sem_post(ul_sem_t *s);
```

---

### 13.3 Queue

A bounded FIFO backed by a shared-memory ring buffer and a notification for
flow control.  Producer and consumer must share the buffer via `ul_mem_grant`.

```c
typedef struct {
	volatile uint32_t head, tail;
	uint32_t          item_size;
	uint32_t          capacity;
	uint8_t           data[];         /* flexible array — item storage */
} ul_queue_buf_t;

typedef struct {
	ul_queue_buf_t *buf;
	ul_notif_t      notif;
} ul_queue_t;

#define UL_QUEUE_HAS_ITEM   (1u << 0)
#define UL_QUEUE_HAS_SPACE  (1u << 1)

/*
 * ul_queue_init - bind @q to a pre-allocated @buf of @buf_size bytes,
 *                 with items of @item_size bytes
 * ul_queue_send - enqueue one item; blocks if full (or returns -UL_ENOSPC
 *                 if @block is false)
 * ul_queue_recv - dequeue one item; blocks if empty (or returns -UL_ENOENT
 *                 if @block is false)
 */
int ul_queue_init(ul_queue_t *q, void *buf, size_t buf_size,
		  uint32_t item_size);
int ul_queue_send(ul_queue_t *q, const void *item, bool block);
int ul_queue_recv(ul_queue_t *q, void *item, bool block);
```

---

### 13.4 Event Group

A notification object **is** an event group.  The `ul_event_*` wrappers
exist only for naming clarity.

```c
typedef ul_notif_t ul_event_t;

/*
 * ul_event_create  - allocate event group (alias for ul_notif_create)
 * ul_event_set     - set bits (alias for ul_notif_signal)
 * ul_event_wait_any - wait until any bit in @mask is set; returns matched bits
 * ul_event_wait_all - wait until ALL bits in @mask are set; returns @mask
 * ul_event_clear   - clear bits without waiting (non-blocking poll + discard)
 */
ul_event_t ul_event_create(void);
void       ul_event_set(ul_event_t ev, uint32_t bits);
uint32_t   ul_event_wait_any(ul_event_t ev, uint32_t mask);
uint32_t   ul_event_wait_all(ul_event_t ev, uint32_t mask);
void       ul_event_clear(ul_event_t ev, uint32_t bits);
```

---

### 13.5 Timer Client

Soft timers are provided by the **timer server** (a userspace thread).
The client API sends IPC requests to the timer server endpoint.

```c
typedef uint16_t ul_timer_id_t;

/* Timer server IPC labels */
#define UL_TIMER_ONESHOT   1
#define UL_TIMER_PERIODIC  2
#define UL_TIMER_CANCEL    3

/*
 * ul_timer_oneshot  - fire @notif/@bit once after @ticks scheduler ticks
 * ul_timer_periodic - fire @notif/@bit every @ticks ticks
 * ul_timer_cancel   - cancel a running timer
 *
 * @svc:   endpoint of the timer server (obtained from service directory)
 * @ticks: expiry delay / period in scheduler ticks
 * @notif: notification to signal on expiry
 * @bit:   bit to set in @notif on expiry
 *
 * Returns timer ID (> 0) or negative error code.
 */
ul_timer_id_t ul_timer_oneshot(ul_ep_t svc, uint32_t ticks,
			       ul_notif_t notif, uint32_t bit);
ul_timer_id_t ul_timer_periodic(ul_ep_t svc, uint32_t ticks,
				ul_notif_t notif, uint32_t bit);
int           ul_timer_cancel(ul_ep_t svc, ul_timer_id_t id);
```

**Timer server thread:** the root thread (or MC Supervisor) is responsible
for spawning the timer server before any client calls `ul_timer_*`.  The timer
server binds the STM0 tick interrupt to its own notification and processes
expired entries after each tick.

---

### 13.6 Pipe

A pipe is a unidirectional queue with a fixed item size of 1 byte.

```c
typedef ul_queue_t ul_pipe_t;

int  ul_pipe_init(ul_pipe_t *p, void *buf, size_t buf_size);
int  ul_pipe_write(ul_pipe_t *p, const uint8_t *data, size_t len, bool block);
int  ul_pipe_read(ul_pipe_t *p, uint8_t *data, size_t len, bool block);
```

---

## 14. Usage Examples

### 14.1 Application thread: IPC client

An app allocates an endpoint it will use as a mailbox, then calls a driver service:

```c
#include <sys/ulipe_microkernel.h>

static void app_main(void *arg)
{
	ul_ep_t svc;
	ul_msg_t msg;
	int ret;

	/* Endpoint handle received from MC Supervisor at startup (via @arg) */
	svc = (ul_ep_t)(uintptr_t)arg;

	msg.label   = 0x0001;          /* command: READ_SENSOR */
	msg.words[0] = 42;             /* sensor channel */

	ret = ul_ep_call(svc, &msg);
	if (ret < 0)
		ul_thread_exit();

	/* msg.words[0] now holds the sensor reading */
	uint32_t reading = msg.words[0];
	(void)reading;

	ul_thread_exit();
}
```

### 14.2 Driver thread: IPC server loop with IRQ

A driver binds a hardware IRQ to a notification, then serves requests
from apps while also handling interrupts:

```c
#include <sys/ulipe_microkernel.h>

#define ASCLIN0_BASE   0xF0000600
#define STM0_IRQ_NUM   10          /* SRC priority assigned at boot */
#define IRQ_BIT        (1u << 0)

static void asclin_driver(void *arg)
{
	ul_ep_t    ep;
	ul_notif_t irq_notif;
	ul_msg_t   msg;
	ul_tid_t   sender;
	uint32_t   bits;
	volatile uint32_t *regs;

	/* Map the peripheral (driver has IO=1 + peripheral capability) */
	regs = ul_mem_map((void *)ASCLIN0_BASE, 256,
			  UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_PERIPH);

	/* Create IPC endpoint so apps can send requests */
	ep = ul_ep_create();

	/* Bind hardware IRQ → notification bit 0 */
	irq_notif = ul_notif_create();
	ul_irq_bind(STM0_IRQ_NUM, irq_notif, 0);
	ul_irq_enable(STM0_IRQ_NUM);

	/* First receive: no previous reply */
	ul_ep_recv(ep, &msg, &sender);

	for (;;) {
		ul_msg_t reply = {0};

		if (msg.label == 0x0001) {          /* WRITE */
			regs[4] = msg.words[0];     /* TX FIFO */
			reply.label   = 0;
			reply.words[0] = 0;         /* OK */
		} else {
			reply.label   = 0xFFFF;     /* unknown command */
			reply.words[0] = (uint32_t)-UL_EINVAL;
		}

		/*
		 * Atomically reply to the current request and wait for the
		 * next IPC message or an IRQ notification.
		 */
		ul_ep_recv_or_notif(ep, irq_notif, IRQ_BIT,
				    &msg, &sender, &bits);

		if (bits & IRQ_BIT) {
			/* IRQ fired — handle TX complete, then re-arm */
			ul_irq_ack(STM0_IRQ_NUM);
			/* ... handle interrupt ... */
			/* Now wait for the next IPC (no previous sender to reply to) */
			ul_ep_recv(ep, &msg, &sender);
		}
	}
}
```

### 14.3 Root thread — full system bootstrap

The root thread creates the MC Supervisor and the timer server, then exits.
The MC Supervisor takes over thread-creation policy from that point on.

```c
#include <sys/ulipe_microkernel.h>

extern void mc_supervisor_entry(void *);
extern void timer_server_entry(void *);

/* The kernel resolves this symbol as the first userspace entry point. */
void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;

	/*
	 * MC Supervisor: highest priority, IO=1.
	 * Receives boot_info so it can manage memory regions.
	 */
	attr = (ul_thread_attr_t){
		.name       = "mc_sup",
		.entry      = mc_supervisor_entry,
		.arg        = (void *)info,
		.priority   = 0,
		.stack_size = 4096,
		.privilege  = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	/* Timer server: medium-high priority, IO=1 (needs IRQ bind). */
	attr = (ul_thread_attr_t){
		.name       = "timer_svc",
		.entry      = timer_server_entry,
		.arg        = NULL,
		.priority   = 4,
		.stack_size = 2048,
		.privilege  = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	/*
	 * Root thread's job is done.  MC Supervisor will create drivers
	 * and app threads via IPC from userspace.
	 */
	ul_thread_exit();
}
```

### 14.4 Using a mutex between two threads

```c
#include <sys/ulipe_microkernel.h>
#include <ul/libul.h>

static ul_mutex_t g_lock;
static uint32_t   g_shared;

static void producer(void *arg)
{
	(void)arg;
	for (;;) {
		ul_mutex_lock(&g_lock);
		g_shared++;
		ul_mutex_unlock(&g_lock);
		ul_thread_yield();
	}
}

static void consumer(void *arg)
{
	(void)arg;
	for (;;) {
		ul_mutex_lock(&g_lock);
		(void)g_shared;
		ul_mutex_unlock(&g_lock);
		ul_thread_yield();
	}
}

/* Called from mc_supervisor after initialisation */
void spawn_shared_example(void)
{
	ul_mutex_init(&g_lock);
	/* ul_thread_create calls omitted for brevity */
}
```

### 14.5 Driver: receiving a soft timer expiry

```c
#include <sys/ulipe_microkernel.h>
#include <ul/libul.h>

#define TIMER_SVC_EP  /* obtained from service directory */  0
#define PERIOD_MS     100   /* ticks = ms when tick_hz = 1000 */
#define TIMER_BIT     (1u << 1)

static void periodic_driver(void *arg)
{
	ul_ep_t      svc    = (ul_ep_t)(uintptr_t)arg;
	ul_notif_t   notif  = ul_notif_create();
	ul_timer_id_t timer = ul_timer_periodic(svc, PERIOD_MS, notif, TIMER_BIT);

	if (timer < 0)
		ul_thread_exit();

	for (;;) {
		ul_notif_wait(notif, TIMER_BIT);
		/* periodic work here */
	}
}
```

---

*This spec is the authoritative reference for the public API.  Implementation
must match it exactly.  Changes require updating this document first.*

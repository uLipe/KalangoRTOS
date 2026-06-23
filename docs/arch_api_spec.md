# ulipeMicroKernel — Architecture Abstraction Layer Specification

**Version:** 0.1 (draft)
**Reference implementation:** TriCore TC1.6.1 / AURIX TC2xx
**Header:** `#include <ul/arch.h>`

---

## Table of Contents

1. [Philosophy and Directory Layout](#1-philosophy-and-directory-layout)
2. [Contract Summary](#2-contract-summary)
3. [Arch Types](#3-arch-types)
4. [Arch Constants and Macros](#4-arch-constants-and-macros)
5. [CPU Control](#5-cpu-control)
6. [Context Management](#6-context-management)
7. [MPU — Memory Protection Unit](#7-mpu--memory-protection-unit)
8. [IRQ and SRC Control](#8-irq-and-src-control)
9. [Tick Timer](#9-tick-timer)
10. [Atomic Operations](#10-atomic-operations)
11. [Boot Entry](#11-boot-entry)
12. [Kernel Callbacks (arch → kernel)](#12-kernel-callbacks-arch--kernel)
13. [TriCore TC2xx Implementation Notes](#13-tricore-tc2xx-implementation-notes)

---

## 1. Philosophy and Directory Layout

The arch layer is a **contract**, not a library.  It separates platform-
independent kernel logic from hardware-specific implementation.  The kernel
calls into the arch layer; the arch layer calls specific kernel callbacks when
hardware events occur.

```
ulipeMicroKernel/
├── kernel/            platform-independent kernel (scheduler, IPC, memory)
├── include/
│   └── ul/
│       └── arch.h     this contract (generic types + function declarations)
└── arch/
    └── tricore/       TriCore TC2xx implementation
        ├── arch_cpu.c
        ├── arch_ctx.S
        ├── arch_mpu.c
        ├── arch_irq.c
        ├── arch_tick.c
        ├── startup.S
        └── include/
            └── arch_config.h   arch-specific constants (UL_ARCH_NUM_DPR, etc.)
```

**Rules:**
- `kernel/` **never** includes `arch/tricore/` headers directly.  It only
  includes `ul/arch.h`.
- `arch/tricore/` may include any TriCore-specific header it needs.
- All arch functions are prefixed `ul_arch_*`.

---

## 2. Contract Summary

Functions the **arch port must implement** (called by the kernel):

| Group | Functions |
|-------|-----------|
| CPU control | `ul_arch_cpu_irq_enable`, `ul_arch_cpu_irq_disable`, `ul_arch_cpu_irq_save`, `ul_arch_cpu_irq_restore`, `ul_arch_cpu_barrier_data`, `ul_arch_cpu_barrier_instr`, `ul_arch_cpu_wait`, `ul_arch_cpu_id` |
| Context | `ul_arch_ctx_init`, `ul_arch_ctx_switch`, `ul_arch_ctx_free`, `ul_arch_csa_pool_init` |
| MPU | `ul_arch_mpu_init`, `ul_arch_mpu_enable`, `ul_arch_mpu_disable`, `ul_arch_mpu_configure`, `ul_arch_mpu_switch` |
| IRQ | `ul_arch_irq_vectors_init`, `ul_arch_irq_src_configure`, `ul_arch_irq_src_enable`, `ul_arch_irq_src_disable`, `ul_arch_irq_src_clear` |
| Tick | `ul_arch_tick_init`, `ul_arch_tick_get` |
| Atomic | `ul_arch_atomic_cas32`, `ul_arch_atomic_load32`, `ul_arch_atomic_store32` |
| Boot | `ul_arch_init` |

Callbacks the **kernel exports** (called by the arch port, e.g., from ISRs):

| Function | When called |
|----------|-------------|
| `ul_kernel_tick()` | Every scheduler tick |
| `ul_kernel_irq_dispatch(srpn)` | On every hardware IRQ |
| `ul_kernel_trap_syscall(tin, args)` | On SYSCALL trap (class 6) |
| `ul_kernel_trap_fault(class, tin)` | On protection fault (class 1) |

---

## 3. Arch Types

Defined in `ul/arch.h`; the kernel uses these opaque types.  The actual struct
layouts are in `arch/<target>/include/arch_config.h`.

```c
/*
 * ul_arch_ctx_t — saved CPU context for a thread
 *
 * TriCore: only PCXI needs to be stored; the CSA chain holds everything else.
 * Other architectures may store a full register frame here.
 */
typedef struct ul_arch_ctx ul_arch_ctx_t;

/*
 * ul_arch_irq_key_t — opaque saved interrupt state
 *
 * Returned by ul_arch_cpu_irq_save(); passed to ul_arch_cpu_irq_restore().
 * TriCore: wraps the ICR register value.
 */
typedef uint32_t ul_arch_irq_key_t;

/*
 * ul_arch_region_t — a single memory protection region descriptor
 *
 * Passed in arrays to ul_arch_mpu_configure().
 */
typedef struct {
	uintptr_t base;     /* region start (must be aligned to UL_ARCH_REGION_ALIGN) */
	size_t    size;     /* region size in bytes (multiple of UL_ARCH_REGION_ALIGN) */
	uint32_t  perms;    /* UL_PERM_READ | UL_PERM_WRITE | UL_PERM_EXEC */
	uint8_t   type;     /* UL_REGION_CODE | UL_REGION_DATA | UL_REGION_PERIPH */
} ul_arch_region_t;

/* Region type tags */
#define UL_REGION_CODE    0
#define UL_REGION_DATA    1
#define UL_REGION_STACK   2
#define UL_REGION_HEAP    3
#define UL_REGION_PERIPH  4
#define UL_REGION_SHARED  5
```

---

## 4. Arch Constants and Macros

Defined in `arch/<target>/include/arch_config.h` and included by `ul/arch.h`.

```c
/*
 * These are defined by the arch port.  Values below are TriCore TC2xx.
 */

/* MPU hardware limits */
#define UL_ARCH_NUM_DPR        18   /* data protection ranges */
#define UL_ARCH_NUM_CPR        10   /* code protection ranges */
#define UL_ARCH_NUM_PRS         4   /* protection register sets */
#define UL_ARCH_MAX_REGIONS    12   /* max regions per thread (data + code) */
#define UL_ARCH_REGION_ALIGN    8   /* minimum region base/size alignment, bytes */

/* PSW initial values for new threads (TriCore PSW register) */
#define UL_ARCH_PSW_USER    0x00000880u  /* IO=0 (user), IS=0, CDE=1, CDC=0 */
#define UL_ARCH_PSW_DRIVER  0x00000980u  /* IO=1 (driver), IS=0, CDE=1, CDC=0 */

/* CSA pool minimum count (each CSA = 64 bytes) */
#define UL_ARCH_CSA_MIN_COUNT  64

/* IPC message registers mapped to CPU data registers */
#define UL_ARCH_SYSCALL_ARG0_REG   "d4"
#define UL_ARCH_SYSCALL_ARG1_REG   "d5"
#define UL_ARCH_SYSCALL_ARG2_REG   "d6"
#define UL_ARCH_SYSCALL_ARG3_REG   "d7"
#define UL_ARCH_SYSCALL_RET_REG    "d2"
```

---

## 5. CPU Control

```c
/*
 * ul_arch_cpu_irq_enable - globally enable CPU interrupt delivery
 *
 * TriCore: `enable` instruction (sets ICR.IE).
 */
void ul_arch_cpu_irq_enable(void);

/*
 * ul_arch_cpu_irq_disable - globally disable CPU interrupt delivery
 *
 * TriCore: `disable` instruction.
 */
void ul_arch_cpu_irq_disable(void);

/*
 * ul_arch_cpu_irq_save - disable interrupts and return previous state
 *
 * Used for critical sections.  The returned key must be passed to
 * ul_arch_cpu_irq_restore() — never compared or manipulated directly.
 *
 * TriCore: reads ICR, calls disable, returns ICR value.
 */
ul_arch_irq_key_t ul_arch_cpu_irq_save(void);

/*
 * ul_arch_cpu_irq_restore - restore interrupt state from a saved key
 *
 * TriCore: MTCR ICR, key.
 */
void ul_arch_cpu_irq_restore(ul_arch_irq_key_t key);

/*
 * ul_arch_cpu_barrier_data - data memory barrier
 *
 * Ensures all pending writes are visible before this point.
 * TriCore: `dsync` instruction.
 */
void ul_arch_cpu_barrier_data(void);

/*
 * ul_arch_cpu_barrier_instr - instruction synchronisation barrier
 *
 * Flushes pipeline; required after writing CSFRs (MTCR).
 * TriCore: `isync` instruction.
 */
void ul_arch_cpu_barrier_instr(void);

/*
 * ul_arch_cpu_wait - enter low-power idle state
 *
 * Returns when an interrupt or any pending request wakes the core.
 * Called from the idle thread when no runnable thread exists.
 *
 * TriCore: `enable` + `wait` — atomically re-enables interrupts and sleeps.
 */
void ul_arch_cpu_wait(void);

/*
 * ul_arch_cpu_id - return the current CPU/core index
 *
 * Returns 0 on single-core systems.
 * TriCore: MFCR CORE_ID & 0x7.
 */
uint32_t ul_arch_cpu_id(void);
```

---

## 6. Context Management

The kernel stores one `ul_arch_ctx_t` per thread.  The arch layer owns the
layout; the kernel treats it as opaque.

```c
/*
 * ul_arch_csa_pool_init - initialise the CSA free list from a memory region
 *
 * @pool_base: physical start of the CSA pool (must be 64-byte aligned)
 * @pool_size: size in bytes; must be a multiple of 64
 *
 * Must be called once before any thread context is created or any interrupt
 * can fire.
 *
 * TriCore: builds the linked list of 64-byte CSA frames and writes FCX/LCX.
 */
void ul_arch_csa_pool_init(uintptr_t pool_base, size_t pool_size);

/*
 * ul_arch_ctx_init - build the initial CPU context for a new thread
 *
 * Fabricates a CSA chain as if the thread had just been interrupted before
 * executing its first instruction.  After this call, ul_arch_ctx_switch()
 * can load the thread without special-casing first runs.
 *
 * @ctx:       context structure to initialise (embedded in the TCB)
 * @entry:     thread entry function
 * @arg:       argument passed in the first argument register (D4 on TriCore)
 * @stack_top: top of the thread's stack (highest address; stack grows down)
 * @priv:      UL_PRIV_USER or UL_PRIV_DRIVER (determines PSW.IO)
 *
 * TriCore: allocates two CSA frames (Lower + Upper) from FCX, fills them
 * with A10=stack_top, A11=entry, D4=arg, PSW=UL_ARCH_PSW_{USER|DRIVER}.
 */
void ul_arch_ctx_init(ul_arch_ctx_t *ctx, void (*entry)(void *), void *arg,
		      uintptr_t stack_top, ul_privilege_t priv);

/*
 * ul_arch_ctx_switch - save current context and load next context
 *
 * Called from the scheduler with interrupts disabled.  On return the CPU is
 * executing the next thread.
 *
 * @prev: context of the thread being suspended (may be NULL on first switch)
 * @next: context of the thread to resume
 *
 * TriCore:
 *   Save path:  SVLCX; MFCR PCXI → prev->pcxi
 *   Load path:  MTCR PCXI ← next->pcxi; ISYNC; RSLCX; RFE
 *
 * Implemented in assembly (arch/tricore/arch_ctx.S).
 */
void ul_arch_ctx_switch(ul_arch_ctx_t *prev, ul_arch_ctx_t *next);

/*
 * ul_arch_ctx_free - return a thread's CSA chain to the free list
 *
 * Called when a thread is destroyed.  Must be called with interrupts disabled.
 *
 * @ctx: the context whose CSA chain will be freed
 *
 * TriCore: walks the chain starting from ctx->pcxi, prepending each 64-byte
 * frame back to FCX.
 */
void ul_arch_ctx_free(ul_arch_ctx_t *ctx);
```

---

## 7. MPU — Memory Protection Unit

The arch MPU layer maps generic region descriptors onto hardware protection
registers.  On TriCore this is the DPR/CPR system with four PRS slots.

```c
/*
 * ul_arch_mpu_init - reset all protection ranges and configure PRS 0 as
 *                    the kernel's unrestricted protection set
 *
 * Called once during boot before ul_arch_mpu_enable().
 *
 * TriCore: zeros all DPR/CPR ranges, sets DPRE/DPWE/CPRE/CPXE = 0xFFFFFFFF
 * for PRS 0 (kernel).  Protection enforcement remains off until
 * ul_arch_mpu_enable() is called.
 */
void ul_arch_mpu_init(void);

/*
 * ul_arch_mpu_enable - activate hardware memory protection (SYSCON.PROTEN)
 *
 * After this call, any access not permitted by the active PRS raises a
 * protection fault (trap class 1).
 */
void ul_arch_mpu_enable(void);

/*
 * ul_arch_mpu_disable - deactivate hardware memory protection
 *
 * Used only during init or fault recovery.  Must not be called from user
 * context.
 */
void ul_arch_mpu_disable(void);

/*
 * ul_arch_mpu_configure - program a PRS slot with a set of regions
 *
 * @prs:     protection register set index (0–UL_ARCH_NUM_PRS-1)
 * @regions: array of region descriptors
 * @count:   number of entries in @regions
 *
 * The function maps regions of type UL_REGION_CODE into CPR entries and all
 * others into DPR entries.  Excess regions (beyond hardware limits) are
 * silently dropped — the caller must ensure count ≤ UL_ARCH_MAX_REGIONS.
 *
 * PRS 0 is reserved for the kernel and must not be reconfigured.
 */
void ul_arch_mpu_configure(uint8_t prs, const ul_arch_region_t *regions,
			   uint8_t count);

/*
 * ul_arch_mpu_switch - switch MPU to the protection set of a thread
 *
 * Called on every context switch, with interrupts disabled.
 *
 * Fast path (pre-assigned PRS): modifies PSW.PRS and PSW.IO only (~3 cycles).
 * Slow path (dynamic PRS 3): calls ul_arch_mpu_configure(3, ...) then
 * updates PSW.
 *
 * @regions: the thread's active region array
 * @count:   number of active regions
 * @prs:     pre-assigned PRS index, or UL_ARCH_NUM_PRS if dynamic
 * @priv:    UL_PRIV_USER or UL_PRIV_DRIVER
 */
void ul_arch_mpu_switch(const ul_arch_region_t *regions, uint8_t count,
			uint8_t prs, ul_privilege_t priv);
```

---

## 8. IRQ and SRC Control

On TriCore, hardware interrupts are routed through SRC (Service Request
Control) registers.  Each peripheral has one or more SRC entries indexed by
their SRPN (Software Request Priority Number, 0–255).

```c
/*
 * ul_arch_irq_vectors_init - set BIV (Interrupt Vector Base) and BTV (Trap
 *                             Vector Base), and configure ISP (Interrupt Stack)
 *
 * @biv:      physical address of the interrupt vector table (must be aligned
 *            to the hardware-required boundary; TriCore: 8 bytes per slot × 256)
 * @btv:      physical address of the trap vector table
 * @isp_top:  top of the interrupt/ISR stack (highest address)
 *
 * Must be called before ul_arch_mpu_enable() and before any interrupt is
 * enabled.
 *
 * TriCore: MTCR BIV, BTV, ISP + ISYNC.
 */
void ul_arch_irq_vectors_init(uintptr_t biv, uintptr_t btv,
			      uintptr_t isp_top);

/*
 * ul_arch_irq_src_configure - configure an SRC entry
 *
 * @srpn: priority number (1–255; 0 is reserved)
 * @tos:  target core (0 = Core 0, 1 = Core 1, 2 = Core 2)
 *
 * Sets the SRPN and TOS fields; does NOT enable the interrupt.
 * Use ul_arch_irq_src_enable() after binding to a notification.
 *
 * The SRC register address for a given SRPN is board-specific and looked up
 * from an arch-private table (not part of the public contract).
 */
void ul_arch_irq_src_configure(uint8_t srpn, uint8_t tos);

/*
 * ul_arch_irq_src_enable  - set SRC.SRE (enables IRQ delivery)
 * ul_arch_irq_src_disable - clear SRC.SRE
 */
void ul_arch_irq_src_enable(uint8_t srpn);
void ul_arch_irq_src_disable(uint8_t srpn);

/*
 * ul_arch_irq_src_clear - clear the pending flag (SRC.CLRR)
 *
 * Must be called inside the kernel's IRQ dispatch path before signalling
 * the userspace notification, to prevent immediate re-entry.
 */
void ul_arch_irq_src_clear(uint8_t srpn);

/*
 * ul_arch_irq_src_trigger - software-trigger an IRQ (SRC.SETR)
 *
 * Used to raise a soft interrupt between cores (inter-core IPC).
 */
void ul_arch_irq_src_trigger(uint8_t srpn);
```

---

## 9. Tick Timer

The tick timer drives the scheduler.  On TriCore TC2xx the System Timer (STM)
is the natural choice: a 64-bit free-running counter with a compare register
that generates an SRC interrupt.

```c
/*
 * ul_arch_tick_init - configure the hardware timer to fire at @freq_hz
 *
 * @freq_hz:   desired tick rate (e.g., 1000 for 1 ms ticks)
 * @tick_srpn: SRC priority number to use for the tick interrupt
 *
 * The implementation must:
 *   1. Program the timer compare register for the given frequency.
 *   2. Configure the SRC entry (ul_arch_irq_src_configure).
 *   3. Enable the SRC (ul_arch_irq_src_enable).
 *
 * The timer ISR calls ul_kernel_tick() and re-arms the compare register.
 *
 * TriCore: uses STM0_CMP0; tick ISR is generated via SRC_STM0_0.
 */
void ul_arch_tick_init(uint32_t freq_hz, uint8_t tick_srpn);

/*
 * ul_arch_tick_get - return the current tick count (monotonically increasing)
 *
 * Thread-safe (reads a volatile counter updated by the tick ISR).
 * May wrap; callers must handle wrapping for timeout arithmetic.
 */
uint32_t ul_arch_tick_get(void);
```

---

## 10. Atomic Operations

These primitives must be implemented without disabling interrupts when the
hardware supports native atomics.  On TriCore TC2xx there is no load-linked /
store-conditional; use `CMPSWAP.W` (compare-and-swap word) instead.

```c
/*
 * ul_arch_atomic_cas32 - compare and swap 32-bit word
 *
 * If *ptr == expected, writes desired to *ptr atomically and returns true.
 * Otherwise returns false; *ptr is unchanged.
 *
 * TriCore: CMPSWAP.W instruction (atomic in terms of bus access).
 */
bool ul_arch_atomic_cas32(volatile uint32_t *ptr, uint32_t expected,
			  uint32_t desired);

/*
 * ul_arch_atomic_load32  - atomic 32-bit load (acquire semantics)
 * ul_arch_atomic_store32 - atomic 32-bit store (release semantics)
 *
 * On architectures with coherent caches, these map to plain load/store with
 * the appropriate barrier.  On TriCore: LD.W / ST.W with DSYNC.
 */
uint32_t ul_arch_atomic_load32(volatile const uint32_t *ptr);
void     ul_arch_atomic_store32(volatile uint32_t *ptr, uint32_t val);
```

---

## 11. Boot Entry

```c
/*
 * ul_arch_init - one-time CPU and peripheral initialisation
 *
 * Called by startup.S immediately before transferring control to the
 * platform-independent kernel.  Must NOT call any kernel functions.
 *
 * Responsibilities:
 *   1. ul_arch_csa_pool_init()  — build CSA free list
 *   2. ul_arch_irq_vectors_init() — set BIV, BTV, ISP
 *   3. ul_arch_mpu_init()       — zero all protection ranges
 *   4. ul_arch_tick_init()      — configure tick timer
 *   5. Return; control passes to ul_kernel_main() [internal kernel fn]
 *
 * The boot information structure (@info) is filled in by this function and
 * passed to ul_kernel_main(), which forwards it to ul_root_thread().
 *
 * @info: caller-allocated ul_boot_info_t to fill in
 */
void ul_arch_init(ul_boot_info_t *info);
```

**startup.S calling convention (TriCore):**

```asm
_start:
    /* Set up kernel stack (A10) and interrupt stack (ISP) */
    movh.a  %a10, hi:_kernel_stack_top
    lea     %a10, [%a10]lo:_kernel_stack_top

    /* ul_arch_init(&boot_info_static) */
    movh.a  %a4, hi:_boot_info_static
    lea     %a4, [%a4]lo:_boot_info_static
    call    ul_arch_init

    /* ul_kernel_main(&boot_info_static)  [does not return] */
    movh.a  %a4, hi:_boot_info_static
    lea     %a4, [%a4]lo:_boot_info_static
    call    ul_kernel_main
    debug
    j .
```

---

## 12. Kernel Callbacks (arch → kernel)

These functions are **implemented by the kernel** and called by the arch port.
They are declared in `ul/arch.h`; their definitions live in `kernel/`.

```c
/*
 * ul_kernel_tick - advance the scheduler clock by one tick
 *
 * Called from the tick timer ISR (inside the interrupt handler, before
 * RSLCX/RFE).  May trigger a context switch; the arch ISR must be
 * structured so that rescheduling takes effect on RFE.
 *
 * The kernel increments the tick counter, unblocks timeout-expired threads,
 * and calls ul_sched_request_preempt() if a higher-priority thread is ready.
 */
void ul_kernel_tick(void);

/*
 * ul_kernel_irq_dispatch - route a hardware IRQ to its notification object
 *
 * @srpn: the SRPN (priority number) of the interrupt that fired
 *
 * Called from the generic ISR stub (isr_common in int_table.S) after SVLCX,
 * before RSLCX/RFE.  The kernel looks up the irq_binding for @srpn, calls
 * ul_arch_irq_src_clear(@srpn), and signals the bound notification.
 */
void ul_kernel_irq_dispatch(uint8_t srpn);

/*
 * ul_kernel_trap_syscall - dispatch a SYSCALL trap (class 6)
 *
 * @tin:  trap identification number (= syscall number, 0–255)
 * @args: pointer to the four syscall arguments (from D4–D7)
 *
 * Called from the trap class 6 handler after SVLCX.  The kernel decodes
 * @tin, validates privilege, executes the syscall, and writes the return
 * value to D2 before returning.
 *
 * TriCore: TIN is in D15 when the trap handler starts.
 */
void ul_kernel_trap_syscall(uint8_t tin, uint32_t args[4]);

/*
 * ul_kernel_trap_fault - handle a hardware protection fault (class 1)
 *
 * @trap_class: hardware trap class (1 for internal protection faults)
 * @tin:        fault subtype
 *              1=PRIV  2=MPR  3=MPW  4=MPX  5=MPP  6=MPN  7=GRWP
 *
 * Called from the trap class 1 handler.  The kernel terminates the faulting
 * thread and triggers rescheduling.
 */
void ul_kernel_trap_fault(uint8_t trap_class, uint8_t tin);
```

---

## 13. TriCore TC2xx Implementation Notes

### 13.1 CSA Pool Sizing

Each CSA frame is 64 bytes.  Every interrupt nesting level and every function
call at trap entry consumes one or two frames.

| Use case | CSA frames consumed |
|----------|---------------------|
| Thread context (Lower + Upper) | 2 |
| Each nested function call | 1 (Upper) |
| Each interrupt nesting level | 2 |

Minimum recommended pool: `UL_ARCH_CSA_MIN_COUNT` (64) = 4 KiB.
For systems with 32 threads and 8 interrupt priorities: 256 frames = 16 KiB.

### 13.2 PRS Allocation Strategy

| PRS | Owner | Notes |
|-----|-------|-------|
| 0 | Kernel | DPRE/DPWE/CPRE/CPXE = 0xFFFFFFFF; never changed |
| 1 | Driver A | Pre-configured for the ASCLIN/SPI driver process |
| 2 | Driver B | Pre-configured for a second driver process |
| 3 | Dynamic | Reconfigured on every switch for remaining threads |

Threads assigned a fixed PRS (0–2) use the fast context-switch path
(3 cycles to change PSW.PRS).  Threads on PRS 3 use the slow path
(≈ 18 writes to CSFR registers per switch).

### 13.3 Region Alignment

TriCore DPR/CPR registers store `lower` and `upper` addresses.  The hardware
enforces no power-of-2 requirement — only 8-byte alignment.  The kernel must
round all region sizes and bases to `UL_ARCH_REGION_ALIGN` (8) before calling
`ul_arch_mpu_configure`.

### 13.4 Syscall Argument Passing

```
SYSCALL #tin  →  Trap Class 6

On entry to the trap handler (after hardware saves Upper Context):
  D4 = arg0    D5 = arg1    D6 = arg2    D7 = arg3
  D15 = TIN    A11 = return address (in upper CSA)

Return value:
  D2 = retval  (written by kernel before RFE)
```

### 13.5 Protection Fault TIN Codes

| TIN | Name | Meaning |
|-----|------|---------|
| 1 | PRIV | Privileged instruction in user mode |
| 2 | MPR  | Memory protection read violation |
| 3 | MPW  | Memory protection write violation |
| 4 | MPX  | Memory protection execute violation |
| 5 | MPP  | Peripheral protection violation |
| 6 | MPN  | Null-pointer access |
| 7 | GRWP | Global register write protection |

### 13.6 W^X Enforcement

The arch layer must silently enforce W^X: when `UL_PERM_WRITE` is present,
`ul_arch_mpu_configure` must clear `UL_PERM_EXEC` from the effective
permissions before writing the CPR/DPR entries.

### 13.7 Key CSFR Addresses

| CSFR | Address | Description |
|------|---------|-------------|
| PCXI | 0xFE00 | Previous context pointer (CSA chain head) |
| PSW  | 0xFE04 | Program status word (IO, PRS, IS, CDC) |
| PC   | 0xFE08 | Program counter (read-only in user mode) |
| SYSCON | 0xFE14 | System configuration (PROTEN bit 1) |
| BIV  | 0xFE20 | Interrupt vector base |
| BTV  | 0xFE24 | Trap vector base |
| ISP  | 0xFE28 | Interrupt stack pointer |
| ICR  | 0xFE2C | Interrupt control register (IE, CCPN) |
| FCX  | 0xFE38 | Free CSA list head |
| LCX  | 0xFE3C | Free CSA list limit (for overflow detection) |
| CORE_ID | 0xFE1C | Current core identification (bits [2:0]) |

---

*This spec is the authoritative contract for all arch ports.  A new port must
implement every function in §5–§11 and invoke every callback in §12 at the
correct point.  The TriCore implementation in `arch/tricore/` is the reference.*

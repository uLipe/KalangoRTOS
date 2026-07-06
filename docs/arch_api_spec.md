# ulmk — Architecture Abstraction Layer Specification

**Version:** 1.0
**Header:** `arch/<arch>/include/ulmk_arch.h`
**Status:** Reflects the implemented contract between `kernel/` and `arch/tricore/`.

> **Purpose of this document:** generic contract that any architecture port must
> satisfy.  Section 13 contains notes specific to the reference TriCore
> implementation.  For porting guidance see `docs/arch_porting_guide.md`.
> For the public userspace API see `docs/api_spec.md`.

---

## Table of Contents

1. [Philosophy and Directory Layout](#1-philosophy-and-directory-layout)
2. [Dependency Rules](#2-dependency-rules)
3. [Arch Types](#3-arch-types)
4. [CPU Control](#4-cpu-control)
5. [Context Management](#5-context-management)
6. [MPU — Memory Protection Unit](#6-mpu--memory-protection-unit)
7. [IRQ Control](#7-irq-control)
8. [Tick Timer](#8-tick-timer)
9. [Atomic Operations](#9-atomic-operations)
10. [Boot Entry](#10-boot-entry)
11. [Character Output Primitive](#11-character-output-primitive)
12. [Kernel Callbacks (arch → kernel)](#12-kernel-callbacks-arch--kernel)
13. [TriCore TC2xx Reference Implementation Notes](#13-tricore-tc2xx-reference-implementation-notes)

---

## 1. Philosophy and Directory Layout

The arch layer is a **contract**, not a library.  It isolates
platform-independent kernel logic from hardware-specific implementation.

```
kernel/          platform-independent (scheduler, IPC, memory, IRQ table)
                 calls ulmk_arch_* only
                 exports ulmk_kernel_* callbacks

arch/
└── <arch>/      one subdirectory per supported ISA
    ├── arch.c           main implementation
    ├── ctx_switch.S     context switch (assembly)
    ├── startup.S        _start, stack, memory init
    ├── vectors.S        trap/ISR entry stubs
    └── include/
        ├── ulmk_arch.h        arch contract header (types + prototypes)
        └── arch_config.h    arch-specific constants (MPU region count, etc.)
```

Adding a new architecture means creating a new `arch/<name>/` subtree that
satisfies every symbol declared in `ulmk_arch.h`.  See `docs/arch_porting_guide.md`.

---

## 2. Dependency Rules

```
kernel/ → arch/ via ulmk_arch.h only
arch/   → kernel/ via ulmk_kernel_* callbacks only
board/  → nothing above it
```

Violations:

- `kernel/` must **never** include arch implementation headers directly.
- `arch/` must **never** include `kernel/` internal headers.
- Arch code must not encode scheduling policy (it calls `ulmk_kern_irq_dispatch()`
  and `ulmk_kern_sched_dispatch()` from generic ISR stubs).
- Kernel code must not contain ISA-specific instructions or register names.

---

## 3. Arch Types

Each arch defines these types in its `ulmk_arch.h`:

### `ulmk_arch_ctx_t` — saved CPU context

```c
typedef struct {
    /* arch-defined fields; must be the FIRST field of ulmk_thread_t */
    ...
} ulmk_arch_ctx_t;
```

Must be the **first field** of `ulmk_thread_t` so the assembly context-switch
path can access it at a known offset without depending on the full TCB layout.

The struct must encode enough state to resume the thread via `ulmk_arch_ctx_switch`.
On register-file architectures this is typically a saved-PC + stack-pointer; on
architectures with hardware-managed context (like TriCore's CSA) it is a link
to the saved frame chain.

### `ulmk_arch_irq_key_t` — saved interrupt state

```c
typedef <arch-defined> ulmk_arch_irq_key_t;
```

Opaque value returned by `ulmk_arch_cpu_irq_save()` and passed to
`ulmk_arch_cpu_irq_restore()`.  Must encode enough state to restore the previous
interrupt enable / mask level exactly.

### `ulmk_arch_region_t` — MPU region descriptor

```c
typedef struct {
    uintptr_t base;
    size_t    size;
    uint32_t  perms;    /* UL_PERM_* bitmask */
    uint8_t   type;     /* UL_REGION_* tag */
} ulmk_arch_region_t;

#define ULMK_REGION_CODE    0
#define ULMK_REGION_DATA    1
#define ULMK_REGION_STACK   2
#define ULMK_REGION_HEAP    3
#define ULMK_REGION_PERIPH  4
#define ULMK_REGION_SHARED  5
```

`perms` uses the same `ULMK_PERM_READ | ULMK_PERM_WRITE | ULMK_PERM_EXEC | ULMK_PERM_USER`
flags defined in `include/ulmk/microkernel.h`.

---

## 4. CPU Control

### `ulmk_arch_cpu_irq_save` / `ulmk_arch_cpu_irq_restore`

```c
ulmk_arch_irq_key_t ulmk_arch_cpu_irq_save(void);
void              ulmk_arch_cpu_irq_restore(ulmk_arch_irq_key_t key);
```

Disable interrupts globally and save/restore the previous state.  The save/restore
pair is re-entrant: nested calls must restore the outermost state only when the
outermost restore is reached.

### `ulmk_arch_cpu_irq_enable` / `ulmk_arch_cpu_irq_disable`

```c
void ulmk_arch_cpu_irq_enable(void);
void ulmk_arch_cpu_irq_disable(void);
```

Unconditional enable / disable.  Used in boot and shutdown paths where no
previous state needs to be preserved.

### `ulmk_arch_cpu_idle`

```c
void ulmk_arch_cpu_idle(void);
```

Enter a low-power wait state until the next interrupt.  Called by the kernel
idle thread.  Must return when an interrupt fires and is handled (i.e., after
the ISR completes).

### `ulmk_arch_cpu_halt`

```c
void ulmk_arch_cpu_halt(void);
```

Hard halt with interrupts disabled.  Used by the panic path.  Must not return.

### `ulmk_arch_cpu_clz`

```c
uint32_t ulmk_arch_cpu_clz(uint32_t val);
```

Count leading zeros of `val`.  Used by the O(1) bitmap scheduler.  Implement
with the native ISA instruction when available; fall back to a software loop.

---

## 5. Context Management

### `ulmk_arch_ctx_init`

```c
void ulmk_arch_ctx_init(ulmk_arch_ctx_t *ctx,
                      void (*entry)(void *arg), void *arg,
                      uintptr_t stack_top, ulmk_privilege_t priv);
```

Fabricate the initial saved context for a new thread.  After this call,
`ulmk_arch_ctx_switch(NULL, ctx)` must start executing `entry(arg)` at privilege
`priv` using the stack whose top is `stack_top`.

Implementations typically push a synthetic register frame onto the stack or
(for hardware-managed context) allocate a saved-context frame in a pool.

A trampoline must ensure that if `entry` returns, `ulmk_thread_exit()` is called.

### `ulmk_arch_ctx_switch`

```c
void ulmk_arch_ctx_switch(ulmk_arch_ctx_t *from, const ulmk_arch_ctx_t *to);
```

Suspend the current thread by saving its CPU state into `*from`, then resume
the thread whose state is in `*to`.  Does not return to the caller until the
current thread is switched back to.

If `from` is NULL, the function performs a one-way jump to `to` (used for the
first task load).

The kernel guarantees that interrupts are disabled when this is called.

### `ulmk_arch_ctx_free`

```c
void ulmk_arch_ctx_free(ulmk_arch_ctx_t *ctx);
```

Release any arch-managed resources associated with `ctx` (e.g., saved-context
frames from a hardware pool).  Called when a thread is killed.  Must be safe to
call from any context with interrupts disabled.

---

## 6. MPU — Memory Protection Unit

### `ulmk_arch_mpu_init`

```c
void ulmk_arch_mpu_init(void);
```

One-time MPU initialisation.  Set up a privileged protection domain (PRS 0 on
TriCore, region 0 on ARMv7-M, etc.) with full kernel access.  Called once from
`ulmk_arch_init()` before the scheduler starts.

### `ulmk_arch_mpu_enable` / `ulmk_arch_mpu_disable`

```c
void ulmk_arch_mpu_enable(void);
void ulmk_arch_mpu_disable(void);
```

Activate / deactivate hardware memory protection.  After `ulmk_arch_mpu_enable()`
returns, accesses outside configured regions must trigger a hardware fault.

### `ulmk_arch_mpu_configure`

```c
void ulmk_arch_mpu_configure(uint8_t prs, const ulmk_arch_region_t *regions,
                            uint8_t count);
```

Program `count` regions into protection domain `prs`.  On architectures with
a Protection Register Set model (e.g., TriCore DPR/CPR), `prs` identifies the
set.  On flat-region architectures (e.g., ARMv7-M MPU), `prs` is a logical
slot that the port maps to hardware regions.

### `ulmk_arch_mpu_switch`

```c
void ulmk_arch_mpu_switch(const ulmk_arch_region_t *regions, uint8_t count,
                         uint8_t prs);
```

Fast domain switch at context switch time.  Must be as cheap as possible; on
architectures that support it (e.g., TriCore `PSW.PRS` field), this is a
single register write.

### `ulmk_arch_mpu_addr_permitted`

```c
bool ulmk_arch_mpu_addr_permitted(uintptr_t addr, size_t size, uint32_t perms);
```

Return true if `[addr, addr+size)` is accessible with `perms` in the currently
active protection domain.  Used by the kernel to validate syscall pointer
arguments before dereferencing them.

---

## 7. IRQ Control

### `ulmk_arch_irq_vectors_init`

```c
void ulmk_arch_irq_vectors_init(uintptr_t trap_table, uintptr_t irq_table,
                               uintptr_t isr_stack_top);
```

Register the trap/interrupt vector tables with hardware and set the ISR stack
pointer.  On architectures where trap and interrupt tables are separate, both
addresses are provided; use the appropriate one for the ISA.

Must be called before `ulmk_arch_cpu_irq_enable()`.

### `ulmk_arch_irq_src_configure`

```c
void ulmk_arch_irq_src_configure(uint8_t srpn, uint8_t priority, uint8_t cpu_id);
```

Configure interrupt source `srpn` (Service Request Priority Number — a logical
index into the interrupt controller).  Set its priority and target CPU.  Leave
it disabled; `ulmk_arch_irq_src_enable()` activates it.

### `ulmk_arch_irq_src_enable` / `ulmk_arch_irq_src_disable`

```c
void ulmk_arch_irq_src_enable(uint8_t srpn);
void ulmk_arch_irq_src_disable(uint8_t srpn);
```

Enable / disable interrupt source `srpn` in the interrupt controller.

### `ulmk_arch_irq_src_ack`

```c
void ulmk_arch_irq_src_ack(uint8_t srpn);
```

Clear the pending flag for `srpn`.  Required after handling a level-triggered
interrupt to prevent immediate re-entry.

### `ulmk_arch_irq_src_is_pending`

```c
bool ulmk_arch_irq_src_is_pending(uint8_t srpn);
```

Return true if `srpn` has a pending interrupt request.

### `ulmk_arch_irq_src_trigger`

```c
void ulmk_arch_irq_src_trigger(uint8_t srpn);
```

Software-trigger interrupt `srpn`.  Used only in test builds to inject
synthetic hardware events.

---

## 8. Hardware IRQ Source Registration

### `ulmk_arch_irq_src_register`

```c
void ulmk_arch_irq_src_register(uint8_t srpn, uint32_t src_reg_addr);
```

Record the absolute address of the SRC register for `srpn`.  Called from the
kernel when userspace invokes `ulmk_irq_bind_hw()`.  The arch layer uses this
mapping in `ulmk_arch_irq_enable()` / `ulmk_arch_irq_ack()`.

There is **no** kernel tick timer.  Periodic timing is implemented in board
services that bind a compare-match IRQ to a notification.

---

## 9. Atomic Operations

### `ulmk_arch_atomic_cas`

```c
uint32_t ulmk_arch_atomic_cas(volatile uint32_t *ptr,
                             uint32_t expected, uint32_t desired);
```

Compare-and-swap.  If `*ptr == expected`, atomically write `desired` and return
`expected`.  Otherwise return the current value of `*ptr` without writing.
Must be lock-free; implement with the native ISA atomic instruction.

### `ulmk_arch_atomic_add`

```c
uint32_t ulmk_arch_atomic_add(volatile uint32_t *ptr, uint32_t val);
```

Atomically add `val` to `*ptr`.  Return the value **before** the add.

---

## 10. Boot Entry

### `ulmk_arch_init`

```c
void ulmk_arch_init(ulmk_boot_info_t *info);
```

One-time CPU and peripheral initialisation.  Called from `startup.S` / `_start`
after `ulmk_board_init()`, `.data` copy, and `.bss` zero.  Fills `*info` and
returns; control passes to `ulmk_kern_main()`.

Mandatory sequence:

1. Initialise the saved-context resource pool (CSA free list, register
   windows, or equivalent).
2. Register trap/interrupt vectors and ISR stack (`ulmk_arch_irq_vectors_init`).
3. Initialise the MPU (`ulmk_arch_mpu_init`).
4. Populate `*info`:
   - `mem[]` / `mem_count`: available physical RAM regions.
   - `csa_pool_base` / `csa_pool_size`: saved-context pool (or zeros if the
     arch does not use a hardware pool).

### `ulmk_board_init`

```c
void ulmk_board_init(void);
```

Board-level early hardware setup.  Called from `_start` **before** `.data` copy.

Constraints:
- No initialised global variables available.
- No `ulmk_*` API calls.
- No interrupt enable.
- Must return normally.

Provided by the board in its `ULMK_BOARD_SOURCES`.  For QEMU / boards that need
no early init this is an empty function.

### `ulmk_kern_main`

```c
void ulmk_kern_main(const ulmk_boot_info_t *info);
```

Platform-independent kernel entry.  Called by `_start` after `ulmk_arch_init()`.
Does not return.  Declared here so `startup.S` can call it without depending on
internal kernel headers.  Must never be called from user code.

---

## 11. Character Output Primitive

```c
void ulmk_printk_char_out(char c);
```

Single-character output to the board's debug console.  Must be provided by the
board as a **strong** (non-weak) symbol in `ULMK_BOARD_SOURCES`.

Called by the kernel printk subsystem and by the arch fault handler before the
scheduler is running.  Must be safe to call very early (before IRQs are
enabled).

---

## 12. Kernel Callbacks (arch → kernel)

These are implemented in `kernel/` and called by the arch port from ISR handlers
and the syscall path.  They contain **no arch-specific code**; all hardware
state must be extracted by the arch before calling.

### `ulmk_kern_irq_dispatch`

```c
void ulmk_kern_irq_dispatch(uint8_t srpn);
```

Route hardware interrupt `srpn` to its bound notification object.  Called from
the generic ISR stub before returning from the interrupt.

### `ulmk_kern_sched_dispatch`

```c
void ulmk_kern_sched_dispatch(bool from_isr);
```

Check whether a higher-priority thread became ready during the trap handler
and yield the CPU before returning to the interrupted context.

- `from_isr == true` — called after `ulmk_kern_irq_dispatch()` on ISR exit.
  TriCore may arm a deferred asm switch; RISC-V performs `ctx_switch` in C.
- `from_isr == false` — called after `ulmk_kern_trap_syscall()` before
  restoring the syscall caller.

**Scheduling contract:**

| Action | API |
|--------|-----|
| Wake a thread (IPC, notif, spawn) | `ulmk_sched_enqueue()` only |
| Voluntary block / yield / exit | `ulmk_sched_resched()` |
| Trap exit preemption | `ulmk_kern_sched_dispatch(from_isr)` |

### `ulmk_kern_trap_syscall`

```c
uint32_t ulmk_kern_trap_syscall(uint8_t tin, uint32_t args[4]);
```

Dispatch a syscall.  `tin` is the syscall number; `args[0..3]` are the four
argument registers read by the syscall entry stub.  Returns the value to be
written as the syscall return value.

### `ulmk_kern_trap_recoverable`

```c
void ulmk_kern_trap_recoverable(void);
```

Kill the current thread after an isolatable hardware fault (e.g., MPU violation
in a user thread).  The arch calls this when the fault can be attributed to a
single thread and the kernel can continue running.

### `ulmk_kern_trap_panic`

```c
void ulmk_kern_trap_panic(void);
```

Halt the system after an unrecoverable fault (kernel fault, ISR fault, or any
fault that cannot be isolated to a single user thread).

---

## 13. TriCore TC2xx Reference Implementation Notes

This section documents implementation decisions specific to the TriCore TC1.6.x
port (`arch/tricore/`).  It is **not** part of the generic contract.

### Files

| File | Implements |
|------|-----------|
| `arch.c` | CPU control, MPU (DPR/CPR), IRQ (SRC), tick (STM0), syscall entry, trap entry |
| `ctx_switch.S` | `ulmk_arch_ctx_switch` — SVLCX/RSLCX/PCXI swap |
| `startup.S` | `_start` — stack, CSA pool, BTV/BIV, `.data`/`.bss`, calls `ulmk_board_init` → `ulmk_arch_init` → `ulmk_kern_main` |
| `vectors.S` | Trap handlers, generic ISR stubs, syscall entry stub |

### `ulmk_arch_ctx_t` on TriCore

```c
typedef struct { uint32_t pcxi; } ulmk_arch_ctx_t;
```

`pcxi` is the PCXI register value encoding a two-frame CSA chain:

```
pcxi → lower-context CSA (UL=0, saved by SVLCX)
      → upper-context CSA (UL=1, saved by hardware on CALL/trap/ISR)
```

### Context switch sequence

```asm
svlcx                    ; save lower context to a free CSA
mfcr   d15, #PCXI        ; read head of saved chain
st.w   [a4], d15         ; from->pcxi = PCXI
ld.w   d15, [a5]         ; d15 = to->pcxi
dsync                    ; flush CSA writes before modifying PCXI
mtcr   #PCXI, d15        ; PCXI = to->pcxi
isync                    ; pipeline sync (mandatory after any MTCR)
rslcx                    ; restore lower context
rfe                      ; restore upper context + PSW + PC
```

### Initial PSW for `ulmk_arch_ctx_init`

```
PSW.IO  = priv    (0=user, 1=driver, 2=supervisor)
PSW.IS  = 0       (thread uses own stack, not ISP)
PSW.CDE = 1
PSW.CDC = 0x7F    (call-depth counter disabled)
PSW.PRS = 0       (kernel PRS; switched at first context switch)
```

`PSW.IS = 0` is critical.  If set to 1, the CPU uses the ISP register as the
stack pointer instead of A10, corrupting the ISR stack.

### CSA pool invariants

- Pool in fast local DSPR RAM only — never in Flash.
- 64-byte aligned; each CSA frame = 64 bytes.
- `ISYNC` after every `MTCR` on any CSFR.
- `DSYNC` before `MTCR #PCXI` when a CSA was written manually.

### IRQ and CCPN

The syscall entry path sets `CCPN = 255` immediately, disabling all hardware
IRQs for the syscall duration.  This avoids the need for IRQ-save critical
sections inside most syscall handlers.

Interrupt sources use SRC (Service Request Control) registers.  The SRC address
for STM0 SR0 is parameterised as `ULMK_ARCH_SRC_STM0_SR0` via board `CFLAGS` to
support different chip families without arch-code changes.

### Trap class routing

| Class | Name | Action |
|-------|------|--------|
| 1 | Internal Protection | `ulmk_kern_trap_recoverable()` |
| 3 TIN 1 | FCD (free list near empty) | log warning, continue |
| 3 TIN 4 | FCU (free list exhausted) | `ulmk_kern_trap_panic()` |
| 6 | System Call | `ulmk_kern_trap_syscall(tin, args)` |
| Others | Various | `ulmk_kern_trap_panic()` |

For the full TriCore hardware reference see `docs/microkernel_book_tricore.md`
and `docs/tricore_guide_pt.md`.

---

## 14. RISC-V RV32 Reference Implementation Notes

This section documents implementation decisions specific to the RISC-V RV32 port
(`arch/riscv/`).  It is **not** part of the generic contract.

### Files

| File | Implements |
|------|-----------|
| `arch.c` | CPU idle (WFI), PMP (`ulmk_arch_mpu_*`), syscall/trap dispatch, atomics |
| `ctx_switch.S` | Software save/restore of callee-saved registers (+ optional FPU) |
| `trap.S` | Machine trap entry, interrupt/exception demux, preempt resume |
| `startup.S` | `_start` — stack, `.data`/`.bss`, `mtvec`, boot chain |
| `irq.c` | IRQ glue to `ulmk_arch_irq_src_*` |
| `irq_clint.c` | MSIP / MTIP (CLINT) backend |
| `irq_plic.c` | PLIC claim/complete backend |

### Privilege and syscalls

- Kernel runs in **M-mode**; user threads run in **U-mode** (`mstatus.MPP=0`).
- Syscalls use `ecall`; number in `a7`, args in `a0`–`a3`, return in `a0`.
- Drivers (`ULMK_PRIV_DRIVER`) enter U-mode via `_ulmk_thread_trampoline_u` + `mret`.

### PMP layout

Static slots (NAPOT): kernel exec, kernel RAM, user text, user RAM, MMIO.
Dynamic slots (`ULMK_ARCH_PMP_USER_BASE`+): per-thread regions from
`ulmk_arch_mpu_switch()`.  `boards/*/memory.ld` must export
`_ulmk_mem_*` symbols for region bounds.

On trap entry the port switches to the kernel PMP layout, then restores the
current thread layout before `mret`.

### IRQ backends

`board.cmake` sets `ULMK_ARCH_HAVE_CLINT` / `ULMK_ARCH_HAVE_PLIC`.
SoC base addresses (`CLINT`, `PLIC`, UART) are **board** constants passed via
`-DULMK_BOARD_*` — not in `arch_config.h`.

Timer drivers must use **MMIO** (`ulmk_mem_map` with `ULMK_MMAP_PERIPH`), not
the `time` CSR (inaccessible from U-mode).

### Preemption from interrupt

RISC-V saves a 128-byte trap frame on the thread stack.  ISR preemption uses
`ulmk_sched_trap_dispatch(true)` → `ulmk_arch_sched_switch()` with
`ULMK_SCHED_SWITCH_COOP` so the switch returns through the trap epilogue on
`mret`.  TriCore arms `g_preempt_*` via `ULMK_SCHED_SWITCH_PREEMPT_ISR`.

See `docs/arch/riscv_implementation.md` for the full RISC-V porting guide.

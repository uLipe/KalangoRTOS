# QEMU Linumiz — MPU Trap Enforcement Gaps

Investigation date: 2026-07-05  
QEMU: `release/ifx/tricore-2.0` (v10.2.94, `tricore_mpu_check` present)  
Test: `tests/memory_isolation_integ`

This document records **known limitations** observed when running the MPU
isolation model on QEMU.  No workarounds are implemented here — only root
cause analysis for scenarios that report `PARTIAL` instead of `PASS`.

## Summary

| Scenario | QEMU result | Root cause class |
|----------|-------------|------------------|
| 1 anon mmap | PASS | URAM static DPR covers heap |
| 2 grant read | PASS | Syscall grant + shared URAM |
| 3 cross-thread write | PARTIAL | No dynamic DPR slots (4-slot limit) |
| 4 kernel exec | PARTIAL | Stale TLB from PRS 0 boot + coarse layout |
| 5 kernel data read | PARTIAL | Stale TLB from PRS 0 boot + coarse layout |

## Scenario 3 — per-thread heap isolation (expected PARTIAL)

With `-DULMK_ARCH_MPU_NUM_DPR=4`, all static slots are consumed:

| Slot | Use |
|------|-----|
| 0 | Kernel bypass (PRS 0) |
| 1 | Kernel RAM (PRS 0 only) |
| 2 | User RAM |
| 3 | Flash read + MMIO |

`ULMK_ARCH_MPU_USER_DPR_BASE == ULMK_ARCH_MPU_NUM_DPR`, so
`mpu_switch()` cannot program per-allocation DPR entries.  All heap blocks
share the same URAM DPR; a write to another thread's buffer does not fault.

**On real silicon** (18 DPR, dynamic slots 6–17): scenario 3 should PASS.  
**On QEMU**: PARTIAL is correct and expected.

## Scenarios 4 & 5 — CPR / KRAM traps not observed

### Expected behaviour

- **Scenario 4**: `calli` to `ulmk_arch_cpu_irq_save` at `0x80002400`
  (`.kernel_text`, CPR 0 only) from a PRS 1 thread → class 1 MPX, thread killed.
- **Scenario 5**: load from `_ulmk_kernel_data_start` (`0x70000000`, DPR 1 /
  KRAM, not in PRS 1 DPRE) → class 0/1 MPR, thread killed.

Disassembly confirms the test really executes `calli` to kernel text; there
is no compiler elision.

### QEMU MPU model (helper.c)

- `tricore_mpu_check()` walks 16 DPR/CPR ranges; active set from
  `(env->PSW >> 12) & 3` (PRS bits [13:12], matches our `arch_config.h`).
- Execute uses **CPXE** bitmasks; data uses **DPRE/DPWE**.
- QEMU CSFR map has **CPXE at 0xE000–0xE00C** only — no separate CPRE
  register.  Our `mpu_write_enables()` maps the `cpre` argument to 0xE000+n×4
  (correct for Linumiz).  Writes to 0xE040+ (`cpxe` argument in our header)
  hit **unimplemented CSFRs** in this QEMU fork (harmless duplicate intent).

### Evidence: `QEMU_LOG=mmu`

Running `memory_isolation_integ` with `-d mmu`:

```
# Early boot (PRS 0 — all static DPRs + kernel CPR enabled)
tricore_cpu_tlb_fill address=0x70000000 ... prot 7   # KRAM — full RWX
tricore_cpu_tlb_fill address=0x80004012 ... prot 7   # kernel .text — full RWX

# After switch to user threads (PRS 1 — new fills)
tricore_cpu_tlb_fill address=0x7003bfc0 ... prot 3   # user pool — RW only
tricore_cpu_tlb_fill address=0xbf000020  ... prot 3   # virt console — RW only
```

`prot 7` = `PAGE_READ | PAGE_WRITE | PAGE_EXEC`.  
`prot 3` = read + write, no execute (correct for data pages at PRS 1).

**No `TLBRET_MPX` / `TLBRET_MPR` lines appear** for kernel text or KRAM during
the test run.

### Root cause: stale TLB across PRS change

1. During boot and kernel init, code runs at **PRS 0** with all DPR/CPR bits
   enabled.  `tricore_cpu_tlb_fill()` caches pages with `prot 7`.
2. User threads start with **PSW.PRS = 1** and PRS 1 enable registers correctly
   restricted (no KRAM DPRE, CPR execute only on user text).
3. **QEMU does not invalidate TLB** when `PSW.PRS` changes or when
   DPRE/DPWE/CPXE registers are rewritten (`tb_flush` / `tlb_flush` not hooked
   in `gen_mtcr` for these CSFRs on this fork).
4. Subsequent instruction fetches or loads to pages already cached at PRS 0
   reuse `prot 7` without re-running `tricore_mpu_check()` under PRS 1.

Hence kernel code execution and KRAM reads succeed without a trap — the test
sets `g_*_progress = 2` (PARTIAL path).

This is a **QEMU emulation gap**, not a kernel logic error in the isolation
layout.  The same linker/MPU configuration should PASS on silicon where TLB
entries are re-evaluated on PRS change (or are not cached across protection
set switches the same way).

### CSFR naming note (informational)

| Our `arch_config.h` | Linumiz QEMU `csfr.h.inc` |
|---------------------|---------------------------|
| `CPRE_n` @ 0xE000+n×4 | `CPXE_n` @ 0xE000+n×4 |
| `CPXE_n` @ 0xE040+n×4 | *(not implemented)* |

Execute enables reach QEMU today only via the `cpre` parameter path in
`mpu_write_enables()`.  Aligning names is a future cleanup, not the primary
cause of scenarios 4/5 PARTIAL (TLB staleness dominates).

## What *does* work on QEMU

- MPU enable (`SYSCON.PROTEN` or any programmed DPRE/DPWE/CPXE).
- Coarse region enforcement for **first touch** at PRS 1 (user pool `prot 3`,
  virt console `prot 3`).
- Class 0/1 trap dispatch (`TRAPC_PROT`, TIN MPR/MPW/MPX) when `tlb_fill`
  runs and check fails.
- Scenarios 1–2 (heap in URAM, grants via syscalls).

## Recommended test interpretation on QEMU

- Treat scenario 3 PARTIAL as **expected** (no dynamic DPR).
- Treat scenarios 4–5 PARTIAL as **QEMU TLB/PRS limitation** until the fork
  flushes TLB on PRS or MPU register changes.
- Re-run scenarios 4–5 on **real TC3xx silicon** before treating failures as
  kernel bugs.

## Suggested upstream/QEMU follow-ups (not implemented here)

1. Flush TLB in QEMU when `PSW.PRS` changes (context switch / RFE).
2. Flush TLB when DPRE/DPWE/CPXE CSFRs are written via `mtcr`.
3. Implement or alias CPRE CSFRs if hardware distinguishes read vs execute
   enable for code regions.

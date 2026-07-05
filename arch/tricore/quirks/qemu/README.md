# arch/tricore/quirks/qemu — QEMU Linumiz Deviations

This directory documents the compile-time overrides required when building
for the Linumiz QEMU-AURIX fork (`qemu-system-tricore -machine KIT_AURIX_TC277_TRB`).
All defaults in `arch/tricore/include/arch_config.h` target TC27x silicon and TSIM.

## Required -D overrides for QEMU

Pass these via `CFLAGS +=` in any test Makefile that targets QEMU:

```makefile
CFLAGS += -DULMK_ARCH_STM0_BASE=0xF0000000u
CFLAGS += -DULMK_ARCH_SRC_STM0_SR0=0xF0038300u
CFLAGS += -DULMK_ARCH_SRC_SRE_BIT=10u
CFLAGS += -DULMK_ARCH_RAM_BASE=0x90000000u
CFLAGS += -DULMK_ARCH_QEMU_VIRT_CONSOLE=1
CFLAGS += -DULMK_ARCH_IDLE_IS_WAIT=0
```

The test Makefiles that include the EMU detection block apply these
overrides automatically in the `else` branch when `EMU=qemu`.

## Deviation Summary

| Symbol                   | TC27x / TSIM default | QEMU Linumiz |
|--------------------------|----------------------|--------------|
| `ULMK_ARCH_STM0_BASE`      | `0xF0001000`         | `0xF0000000` |
| `ULMK_ARCH_SRC_STM0_SR0`   | `0xF0038490`         | `0xF0038300` |
| `ULMK_ARCH_SRC_SRE_BIT`    | `12`                 | `10`         |
| `ULMK_ARCH_RAM_BASE`       | `0x70000000`         | `0x90000000` |
| `ULMK_ARCH_QEMU_VIRT_CONSOLE` | `0`               | `1`          |
| `ULMK_ARCH_IDLE_IS_WAIT`   | `1`                  | `0`          |

## VIRT Device (console + exit)

QEMU provides a virtual device at `0xBF000000`:
- `+0x20`: write any byte → emit on stdout
- `+0x28`: write exit code → `exit(code)` in the hypervisor

`boards/qemu_tc27x/qemu_console.c` implements `ulmk_printk_char_out()` and
`ulmk_sim_exit()` using this device.

Without `ULMK_ARCH_QEMU_VIRT_CONSOLE=1`, the MPU init skips the DPR covering
`0xBF000000`, which causes a Class-1 trap when a driver-privilege thread
calls `ulmk_printk`.

## STM0 Base Address

QEMU maps STM0 at `0xF0000000` (no offset within the peripheral window),
while TC27x silicon and TSIM use `0xF0001000`.  The offset is due to the
Linumiz `tc27xd_soc` model placing STM0 immediately at the peripheral base.

## SRC Register Layout

QEMU uses a flat slot array: `SRC_STM0_SR0 = SRC_BASE + 0×300` (slot 0xC0).
TC27x hardware uses `SRC_STM0_SR0 = 0xF0038490`.

The SRE (Service Request Enable) bit is at position **10** in the QEMU IR
model but at position **12** in the TC27x SRC hardware register.

## WAIT Instruction

QEMU TC277 does not wake the CPU from `WAIT` on timer compare match,
causing the idle thread to hang and blocking the scheduler tick.
Set `ULMK_ARCH_IDLE_IS_WAIT=0` when targeting QEMU to use `NOP` instead.

## CSA / CDC

QEMU initialises `PSW.CDE=1` at reset.  Any `CALL` chain increments CDC,
and an `RFE` from a `CALL` (not ISR) context traps NEST when `CDC != 0`.
The context switch in `ctx_switch.S` and `vectors.S` explicitly clears CDC
(bits `[6:0]` of PSW) before each `RSLCX + RFE`.  This is also required on
real silicon but is more visible in QEMU because `CDE` is always 1.

## MPU

QEMU Linumiz emulates TriCore MPU protection (class-1 internal protection
traps on DPR/CPR violations).  `tricore_mpu_check()` walks **16 logical**
DPR/CPR ranges per access (`MPU_NUM_RANGES` in `helper.c`).

**CSFR addressing:** our port uses the TC2xx sequential layout (`DPRn_L =
0xC000 + n×8`, `CPRn_L = 0xD000 + n×8`).  The emulator maps only the first
group to implemented CSFRs:

| Slot (n) | DPR CSFR | CPR CSFR | QEMU env field |
|----------|----------|----------|----------------|
| 0 | 0xC000 | 0xD000 | DPR0_0 / CPR0_0 |
| 1 | 0xC008 | 0xD008 | DPR0_1 / CPR0_1 |
| 2 | 0xC010 | 0xD010 | DPR0_2 / CPR0_2 |
| 3 | 0xC018 | 0xD018 | DPR0_3 / CPR0_3 |
| 4+ | 0xC020+ | 0xD020+ | unimplemented (writes ignored) |

QEMU builds therefore set both:

```
-DULMK_ARCH_MPU_NUM_DPR=4
-DULMK_ARCH_MPU_NUM_CPR=4
```

via `boards/qemu_tc3xx/board.cmake`.

**Minimum isolation layout (4 DPR + 2 CPR used):**

| Slot | Role | PRS 1 |
|------|------|-------|
| DPR 0 | kernel bypass 4 GiB | off |
| DPR 1 | kernel RAM | off |
| DPR 2 | user RAM | R+W |
| DPR 3 | flash read + MMIO | R+W |
| CPR 0 | kernel exec | X off |
| CPR 1 | user exec | X on |

`ULMK_ARCH_MPU_USER_DPR_BASE == 4` on QEMU — no spare DPR for per-thread
dynamic regions; `mpu_switch()` preserves the static layout and ignores extra
regions from mmap.  Per-thread heap isolation requires real silicon (18 DPR,
dynamic slots from index 6).

See `TRAP_LIMITATIONS.md` for PASS vs PARTIAL scenarios and TLB/PRS gaps.

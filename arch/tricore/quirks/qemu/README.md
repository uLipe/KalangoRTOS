# QEMU Linumiz Quirks — boards/qemu_tc3xx

Platform overrides for the Linumiz `qemu-system-tricore` fork live in
`boards/qemu_tc3xx/board_config.h` (not in `arch/tricore/arch_config.h`).

## board_config.h vs TC27x silicon

| Symbol | TC27x silicon | QEMU (`board_config.h`) |
|--------|---------------|-------------------------|
| `ULMK_BOARD_STM0_BASE` | `0xF0001000` | `0xF0001000` |
| `ULMK_BOARD_SRC_STM0_SR0` | `0xF0038490` | `0xF0038300` (slot 0xC0) |
| `ULMK_BOARD_SRC_SRE_BIT` | `12` | `10` |
| `ULMK_BOARD_RAM_BASE` | `0x70000000` | `0x70000000` |
| `ULMK_BOARD_HAVE_VIRT_CONSOLE` | `0` | `1` |
| `ULMK_BOARD_IDLE_IS_WAIT` | `1` | `0` |
| `ULMK_BOARD_MPU_NUM_DPR` | `18` | `4` |

Real silicon boards supply their own `board_config.h` under `ULMK_CHIP_DIR`.

## STM0 base

QEMU may map STM0 at `0xF0000000` on some models (no offset within the
peripheral window).  Override `ULMK_BOARD_STM0_BASE` in board_config if needed.

## SRC layout

QEMU uses a flat slot array: `SRC = ULMK_BOARD_SRC_BASE + slot * 4`.
TC27x hardware uses fixed SRC register addresses per peripheral.

## WAIT idle

Use `ULMK_BOARD_IDLE_IS_WAIT = 0` in QEMU builds so the instruction counter
advances between timer interrupts.

## MPU

`ULMK_BOARD_MPU_NUM_DPR == 4` on QEMU — no spare DPR for per-thread dynamic
regions; extra mmap regions are tracked in the kernel but ignored by arch.

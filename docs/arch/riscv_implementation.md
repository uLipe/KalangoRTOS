# RISC-V RV32 port — ulmk

Target: **rv32imac** on QEMU `virt` (`boards/qemu_riscv_virt`).

## Layout

| Layer | Path |
|-------|------|
| Arch | `arch/riscv/` — startup, trap, ctx_switch, PMP, IRQ backends |
| Board | `boards/qemu_riscv_virt/` — UART 16550, Goldfish RTC timer, test finisher |
| Toolchain | xpack `riscv-none-elf-gcc` (see `tools/docker/Dockerfile`) |

## Privilege and isolation

- Kernel runs in **M-mode**; user/driver threads enter **U-mode** via `mret` from `_ulmk_thread_trampoline`.
- Memory isolation uses **PMP** (NAPOT regions) via the frozen `ulmk_arch_mpu_*` API.
- Static PMP slots: kernel text, kernel RAM, user text, user RAM, MMIO window.
- Dynamic per-thread regions use slots from `ULMK_ARCH_PMP_USER_BASE`.

## Interrupt controllers

Backends are separate compilation units, selected in `board.cmake`:

| File | `ULMK_ARCH_HAVE_*` | Role |
|------|-------------------|------|
| `irq_clint.c` | `ULMK_ARCH_HAVE_CLINT=1` | MSIP/MTIMER via `mie` (legacy) |
| `irq_clic.c` | `ULMK_ARCH_HAVE_CLIC=1` | Core-local MMIO interrupts |
| `irq_plic.c` | `ULMK_ARCH_HAVE_PLIC=1` | MEIP claim/complete, peripheral IRQs |
| `irq.c` | (glue) | `ulmk_arch_irq_src_*`, trap demux |

SoC base addresses (`ULMK_BOARD_CLIC_BASE`, `ULMK_BOARD_PLIC_BASE`, UART) are defined in **board** headers / `board.cmake`, not in `arch_config.h`.

## Timer (board service)

The sleep server uses the QEMU **Goldfish RTC** (`0x00101000`) as an MMIO compare timer. The compare-match IRQ is delivered through **PLIC** (IRQ 11), with `ulmk_notif_wait()` in the server thread — same pattern as STM0 on TriCore. CLINT `mtime`/`mtimecmp` is not used.

## Build

```bash
python3 tools/dev.py build --board boards/qemu_riscv_virt
python3 tools/dev.py build qemu --board boards/qemu_riscv_virt
```

## Tests

```bash
python3 tools/dev.py tests integ --board boards/qemu_riscv_virt
```

`csa_ctx` is TriCore-only (no hardware CSA pool on RISC-V).

## QEMU map (virt)

| Device | Address |
|--------|---------|
| RAM/code | `0x80000000` |
| CLIC (M-mode) | `0x02000000` (boards with `ULMK_ARCH_HAVE_CLIC=1`; not on stock QEMU virt yet) |
| PLIC | `0x0C000000` |
| Goldfish RTC | `0x00101000` |
| UART0 | `0x10000000` |
| Test finisher | `0x00100000` |

## Future

- Board `sifive_u` as HiFive Unleashed proxy (phase 2).
- FPU: `-DULMK_ARCH_HAVE_FPU=1` + `-march=rv32imafc`.

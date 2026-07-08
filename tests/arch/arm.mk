# tests/arch/arm.mk — ARM Cortex-M integration-test toolchain
#
# Defaults to the ARMv7-M board (mps2-an500, Cortex-M7).  Override for the
# ARMv8-M board (mps2-an505, Cortex-M33):
#     make ARCH=arm ARM_BOARD=qemu_mps2_an505 ...

ARM_BOARD ?= qemu_mps2_an500

CC      := arm-none-eabi-gcc
AR      := arm-none-eabi-ar
QEMU    := qemu-system-arm
BOARD   := $(ROOT)/boards/$(ARM_BOARD)
TEST_LD := boot_test_arm.ld

ifeq ($(ARM_BOARD),qemu_mps2_an505)
MACHINE    := mps2-an505
ARM_MCPU   := -mcpu=cortex-m33 -mfloat-abi=softfp -mfpu=fpv5-sp-d16
# The AN505 boots Secure and resets from the Secure code alias; every region
# uses the Secure (IDAU bit 28 set) alias.  ssram-1 (0x38000000) holds data.
ARM_FLASH_BASE  := 0x10000000
ARM_SRAM_BASE   := 0x38000000
ARM_PERIPH_BASE := 0x50000000
else
MACHINE    := mps2-an500
ARM_MCPU   := -mcpu=cortex-m7 -mfloat-abi=softfp -mfpu=fpv5-sp-d16
ARM_FLASH_BASE  := 0x00000000
ARM_SRAM_BASE   := 0x20000000
ARM_PERIPH_BASE := 0x40000000
endif

# -semihosting lets ulmk_sim_exit() (SYS_EXIT) stop the machine with a status.
QEMU_EXTRA := -semihosting

ARCH_CFLAGS  := -mthumb $(ARM_MCPU)
# The shared boot_test_arm.ld takes its MEMORY origins from these symbols.
ARCH_LDFLAGS := -mthumb $(ARM_MCPU) \
	-Wl,--defsym=ULMK_FLASH_BASE=$(ARM_FLASH_BASE) \
	-Wl,--defsym=ULMK_SRAM_BASE=$(ARM_SRAM_BASE) \
	-Wl,--defsym=ULMK_PERIPH_BASE=$(ARM_PERIPH_BASE)

# Both MPU backends are listed; each is #if-guarded on ULMK_ARCH_ARMV8M so only
# the one matching the board contributes code.
ARCH_KERNEL_SRCS := \
	$(ROOT)/arch/arm/startup.S \
	$(ROOT)/arch/arm/vectors.S \
	$(ROOT)/arch/arm/trap.S \
	$(ROOT)/arch/arm/ctx_switch.S \
	$(ROOT)/arch/arm/arch.c \
	$(ROOT)/arch/arm/irq.c \
	$(ROOT)/arch/arm/mpu_v7m.c \
	$(ROOT)/arch/arm/mpu_v8m.c \
	$(BOARD)/qemu_printk_hook.c

ARCH_INCLUDE := -I$(ROOT)/arch/arm/include

BOARD_EXTRA_SRCS :=

ARCH_TEST_IO := $(INTEG_DIR)arch/arm_test_io.c

# ulmk_board_init provider.  A board with a dedicated board_init.c (an505: PPC
# unprivileged enable) must link it in *every* build — including tests that skip
# the board service threads — because the HW bring-up is mandatory for any
# unprivileged peripheral access, not just for the console/timer servers.
BOARD_INIT_SRC := $(wildcard $(BOARD)/board_init.c)

ifneq ($(ARCH_SKIP_BOARD_SVC),1)
# Board services on: integ_common.mk links board_services.c.  Boards that keep
# ulmk_board_init there (an500) need nothing extra; boards that split it out
# (an505) contribute board_init.c here — board_services.c no longer defines it.
INTEG_KERNEL_STUB := $(BOARD_INIT_SRC)
else
# Board services skipped: link the board's own board_init.c when present, else
# the generic no-op stub.
ifneq ($(BOARD_INIT_SRC),)
INTEG_KERNEL_STUB := $(BOARD_INIT_SRC)
else
INTEG_KERNEL_STUB := $(ROOT)/stub/board_init_stub.c
endif
endif

# cmake/arch_sources.cmake — arch-specific source lists (included after arch.cmake).

if(ULMK_ARCH STREQUAL "tricore")
	set(ULMK_ARCH_KERNEL_SOURCES
		${ULMK_ARCH_DIR}/arch.c
		${ULMK_ARCH_DIR}/ctx_switch.S)
	set(ULMK_ARCH_EXE_SOURCES
		${ULMK_ARCH_DIR}/startup.S
		${ULMK_ARCH_DIR}/vectors.S)
elseif(ULMK_ARCH STREQUAL "riscv")
	set(ULMK_ARCH_KERNEL_SOURCES
		${ULMK_ARCH_DIR}/arch.c
		${ULMK_ARCH_DIR}/irq.c
		${ULMK_ARCH_DIR}/irq_clint.c
		${ULMK_ARCH_DIR}/irq_clic.c
		${ULMK_ARCH_DIR}/irq_plic.c
		${ULMK_ARCH_DIR}/ctx_switch.S
		${ULMK_ARCH_DIR}/trap.S)
	set(ULMK_ARCH_EXE_SOURCES
		${ULMK_ARCH_DIR}/startup.S)
else()
	message(FATAL_ERROR "Unsupported ULMK_ARCH=${ULMK_ARCH}")
endif()

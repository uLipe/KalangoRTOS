/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Platform-independent kernel entry — kernel/kernel_main.c
 * Called by startup.S after ul_arch_init() fills the boot_info.
 * Does not return.
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <ul_arch.h>
#include <kernel/include/ul_sched.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_irq_internal.h>
#include <kernel/include/ul_timer_internal.h>
#include <kernel/include/ul_mem_internal.h>
#include <kernel/include/ul_printk.h>
#include <kernel/syscall/syscall_router.h>

/* Linker-provided user pool boundaries (defined in .user_pool section). */
extern uint8_t _ul_user_pool_start[];
extern uint8_t _ul_user_pool_end[];

/* =========================================================================
 * Arch callbacks — invoked from the arch layer (ISR/trap stubs)
 * ========================================================================= */

void ul_kernel_tick(void)
{
	ul_timer_tick();
	ul_sched_tick();
}

void ul_kernel_irq_check_preempt(void)
{
	ul_sched_check_preempt();
}

/*
 * ul_kernel_syscall_check_preempt — called at the end of every syscall,
 * before restoring the caller's context.
 *
 * If a higher-priority thread became ready during the syscall (e.g. a
 * notification was delivered, a thread was resumed), the caller is
 * demoted to READY and ul_sched_schedule() switches to the new thread.
 * Execution resumes here when the caller is eventually rescheduled, then
 * returns normally to ul_arch_syscall_entry() and thence to userspace.
 *
 * The running thread is never removed from the run queue by pick_next(),
 * so no ul_sched_enqueue() is needed — just mark it READY and schedule.
 */
void ul_kernel_syscall_check_preempt(void)
{
	ul_thread_t *cur  = ul_sched_current();
	ul_thread_t *next = ul_sched_peek_next();

	if (!cur || !next || next == cur || next->priority >= cur->priority)
		return;

	cur->state = UL_THREAD_STATE_READY;
	ul_sched_schedule();
}

uint32_t ul_kernel_trap_syscall(uint8_t tin, uint32_t args[4])
{
	return ul_syscall_router(tin, args[0], args[1], args[2], args[3]);
}

void ul_kernel_trap_recoverable(void)
{
	ul_thread_t *cur = ul_sched_current();

	if (cur) {
		ul_printk("TRAP: killing thread tid=%u\n",
			  (unsigned)cur->tid);
		cur->state = UL_THREAD_STATE_DEAD;
		ul_sched_dequeue(cur);
		ul_sched_schedule();
	}

	for (;;)
		;
}

void ul_kernel_trap_panic(void)
{
	ul_printk("KERNEL PANIC: unrecoverable trap\n");
	for (;;)
		;
}

/* =========================================================================
 * Thread entries
 * ========================================================================= */

static void idle_thread_entry(void *arg)
{
	(void)arg;
	for (;;)
		ul_arch_cpu_idle();
}

static void root_thread_entry(void *arg)
{
	ul_printk("ulipeMicroKernel: root thread\n");
	ul_root_thread((const ul_boot_info_t *)arg);
	ul_thread_exit();
}

/* =========================================================================
 * Static thread storage
 * ========================================================================= */

static ul_thread_t idle_thread_g   UL_KERNEL_BSS;
static uint8_t     idle_stack_g[256]
	__attribute__((aligned(8))) UL_KERNEL_BSS;

static ul_thread_t root_thread_g   UL_KERNEL_BSS;
static uint8_t     root_stack_g[4096]
	__attribute__((aligned(8))) UL_KERNEL_BSS;

/* =========================================================================
 * Kernel main — does not return
 * ========================================================================= */

void ul_kernel_main(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;

	ul_printk("ulipeMicroKernel: kernel entry\n");

	ul_sched_init();
	ul_sched_set_class(&ul_fifo_rt_class);
	UL_LOG_DBG("sched init done");

	ul_irq_table_init();
	UL_LOG_DBG("irq table init done");

	ul_arch_mpu_init();
	ul_arch_tick_init();
	ul_timer_init();
	ul_arch_cpu_irq_enable();
	UL_LOG_DBG("timer init done");

	ul_heap_init((uintptr_t)_ul_user_pool_start,
		     (uintptr_t)_ul_user_pool_end - (uintptr_t)_ul_user_pool_start);
	ul_printk("ulipeMicroKernel: heap %u bytes available\n",
		  (unsigned)ul_heap_free_bytes());

	/*
	 * Create the idle thread first — lowest priority (255), always in
	 * the run queue.  The scheduler switches to it when no real thread
	 * is ready, eliminating the need for a special idle context or a
	 * polling loop in the kernel main frame.
	 */
	attr.name       = "idle";
	attr.entry      = idle_thread_entry;
	attr.arg        = NULL;
	attr.priority   = 255u;
	attr.stack_size = sizeof(idle_stack_g);
	attr.privilege  = UL_PRIV_KERNEL;
	ul_thread_init(&idle_thread_g, &attr, idle_stack_g);
	ul_sched_enqueue(&idle_thread_g);

	attr.name       = "root";
	attr.entry      = root_thread_entry;
	attr.arg        = (void *)info;
	attr.priority   = 0u;
	attr.stack_size = sizeof(root_stack_g);
	attr.privilege  = UL_PRIV_KERNEL;
	ul_thread_init(&root_thread_g, &attr, root_stack_g);
	ul_sched_enqueue(&root_thread_g);

#ifdef UL_KERNEL_PRE_ROOT_HOOK
	{
		extern void ul_kernel_pre_root_hook(void);
		ul_kernel_pre_root_hook();
	}
#endif

	ul_arch_mpu_enable();
	UL_LOG_DBG("mpu enabled");

	ul_printk("ulipeMicroKernel: switching to root thread\n");

	/*
	 * ul_sched_start() picks the highest-priority ready thread
	 * (root_thread_g, prio=0) and performs the initial context switch.
	 * It does not return.
	 */
	ul_sched_start();

	for (;;)
		;
}

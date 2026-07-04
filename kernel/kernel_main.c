/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Platform-independent kernel entry — kernel/kernel_main.c
 * Called by startup.S after ulmk_arch_init() fills the boot_info.
 * Does not return.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <ulmk/config.h>
#include <ulmk_arch.h>
#include <kernel/include/ulmk_sched.h>
#include <kernel/include/ulmk_thread_internal.h>
#include <kernel/include/ulmk_irq_internal.h>
#include <kernel/include/ulmk_timer_internal.h>
#include <kernel/include/ulmk_mem_internal.h>
#include <kernel/include/ulmk_printk.h>
#include <kernel/syscall/syscall_router.h>

/* Linker-provided user pool boundaries (defined in .user_pool section). */
extern uint8_t _ulmk_user_pool_start[];
extern uint8_t _ulmk_user_pool_end[];

/* =========================================================================
 * Arch callbacks — invoked from the arch layer (ISR/trap stubs)
 * ========================================================================= */

void ulmk_kern_tick(void)
{
	ulmk_timer_tick();
	ulmk_sched_tick();
}

void ulmk_kern_irq_check_preempt(void)
{
	ulmk_sched_check_preempt();
}

/*
 * ulmk_kern_syscall_check_preempt — called at the end of every syscall,
 * before restoring the caller's context.
 *
 * If a higher-priority thread became ready during the syscall (e.g. a
 * notification was delivered, a thread was spawned), the caller is
 * re-enqueued as READY and ulmk_sched_schedule() switches to the new thread.
 * Execution resumes here when the caller is eventually rescheduled, then
 * returns normally to ulmk_arch_syscall_entry() and thence to userspace.
 */
void ulmk_kern_syscall_check_preempt(void)
{
	ulmk_thread_t *cur  = ulmk_sched_current();
	ulmk_thread_t *next = ulmk_sched_peek_next();

	if (!cur || !next || next == cur || next->priority >= cur->priority)
		return;

	cur->state = UL_THREAD_STATE_READY;
	ulmk_sched_enqueue(cur);
	ulmk_sched_schedule();
}

uint32_t ulmk_kern_trap_syscall(uint8_t tin, uint32_t args[4])
{
	return ulmk_syscall_router(tin, args[0], args[1], args[2], args[3]);
}

void ulmk_kern_trap_recoverable(void)
{
	ulmk_thread_t *cur = ulmk_sched_current();

	if (cur) {
		ulmk_printk("TRAP: killing thread tid=%u\n",
			  (unsigned)cur->tid);
		cur->state = UL_THREAD_STATE_DEAD;
		ulmk_sched_dequeue(cur);
		ulmk_sched_schedule();
	}

	for (;;)
		;
}

void ulmk_kern_trap_panic(void)
{
	ulmk_printk("KERNEL PANIC: unrecoverable trap\n");
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
		ulmk_arch_cpu_idle();
}

static void root_thread_entry(void *arg)
{
	ulmk_printk("ulmk: root thread\n");
	ulmk_root_thread((const ulmk_boot_info_t *)arg);
	ulmk_thread_exit();
}

/* =========================================================================
 * Static thread storage
 * ========================================================================= */

static ulmk_thread_t idle_thread_g   UL_KERNEL_BSS;
static uint8_t     idle_stack_g[256]
	__attribute__((aligned(8))) UL_KERNEL_BSS;

static ulmk_thread_t root_thread_g   UL_KERNEL_BSS;
static uint8_t     root_stack_g[4096]
	__attribute__((aligned(8))) UL_KERNEL_BSS;

/* =========================================================================
 * Kernel main — does not return
 * ========================================================================= */

void ulmk_kern_main(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};

	ulmk_printk("ulmk: kernel entry\n");

	ulmk_sched_init();
	ulmk_sched_set_class(&ulmk_bitmap_rt_class);
	UL_LOG_DBG("sched init done");

	ulmk_irq_table_init();
	UL_LOG_DBG("irq table init done");

	ulmk_arch_mpu_init();
	ulmk_arch_tick_init();
	ulmk_timer_init();
	ulmk_arch_cpu_irq_enable();
	UL_LOG_DBG("timer init done");

	ulmk_heap_init((uintptr_t)_ulmk_user_pool_start,
		     (uintptr_t)_ulmk_user_pool_end - (uintptr_t)_ulmk_user_pool_start);
	ulmk_printk("ulmk: heap %u bytes available\n",
		  (unsigned)ulmk_heap_free_bytes());

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
	attr.privilege  = ULMK_PRIV_KERNEL;
	ulmk_thread_init(&idle_thread_g, &attr, idle_stack_g);
	ulmk_sched_enqueue(&idle_thread_g);

	attr.name       = "root";
	attr.entry      = root_thread_entry;
	attr.arg        = (void *)info;
	attr.priority   = 0u;
	attr.stack_size = sizeof(root_stack_g);
	attr.privilege  = ULMK_PRIV_KERNEL;
	ulmk_thread_init(&root_thread_g, &attr, root_stack_g);
	ulmk_sched_enqueue(&root_thread_g);

#ifdef UL_KERNEL_PRE_ROOT_HOOK
	{
		extern void ulmk_kernel_pre_root_hook(void);
		ulmk_kernel_pre_root_hook();
	}
#endif

	ulmk_arch_mpu_enable();
	UL_LOG_DBG("mpu enabled");

	ulmk_printk("ulmk: switching to root thread\n");

	/*
	 * ulmk_sched_start() picks the highest-priority ready thread
	 * (root_thread_g, prio=0) and performs the initial context switch.
	 * It does not return.
	 */
	ulmk_sched_start();

	for (;;)
		;
}

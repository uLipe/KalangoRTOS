#!/usr/bin/env python3
"""
Rename ul_ → ulmk_ across the ulipeMicroKernel codebase.

Run from the repo root:
    python3 tools/rename_ulmk.py

Applies ordered string replacements to every relevant source, cmake,
linker, doc, and cursor-rule file. Does NOT touch reference books or
binary files.
"""
import os
import sys

# --- Ordered replacements (more specific BEFORE less specific) -----------

REPLACEMENTS = [
    # ── Kernel callbacks (arch → kernel): ul_kernel_ → ulmk_kern_ ────────
    ("ul_kernel_trap_syscall",        "ulmk_kern_trap_syscall"),
    ("ul_kernel_trap_recoverable",    "ulmk_kern_trap_recoverable"),
    ("ul_kernel_trap_panic",          "ulmk_kern_trap_panic"),
    ("ul_kernel_irq_dispatch",        "ulmk_kern_irq_dispatch"),
    ("ul_kernel_irq_check_preempt",   "ulmk_kern_sched_dispatch"),
    ("ul_kernel_syscall_check_preempt","ulmk_kern_sched_dispatch"),
    ("ul_kernel_tick",                "ulmk_kern_tick"),
    ("ul_kernel_main",                "ulmk_kern_main"),

    # ── Arch namespace ────────────────────────────────────────────────────
    ("ul_arch_",       "ulmk_arch_"),

    # ── Boot symbols ──────────────────────────────────────────────────────
    ("ul_board_init",        "ulmk_board_init"),
    ("ul_printk_char_out",   "ulmk_printk_char_out"),
    ("ul_printk",            "ulmk_printk"),
    ("ul_root_thread",       "ulmk_root_thread"),

    # ── Public API functions ──────────────────────────────────────────────
    ("ul_thread_",      "ulmk_thread_"),
    ("ul_ep_",          "ulmk_ep_"),
    ("ul_notif_",       "ulmk_notif_"),
    ("ul_mem_map",      "ulmk_mem_map"),
    ("ul_mem_unmap",    "ulmk_mem_unmap"),
    ("ul_mem_grant",    "ulmk_mem_grant"),
    ("ul_irq_",         "ulmk_irq_"),
    ("ul_timer_",       "ulmk_timer_"),
    ("ul_cap_",         "ulmk_cap_"),
    ("ul_malloc",       "ulmk_malloc"),
    ("ul_free",         "ulmk_free"),
    ("ul_aligned_alloc","ulmk_aligned_alloc"),
    ("ul_sched_",       "ulmk_sched_"),
    ("ul_ipc_",         "ulmk_ipc_"),

    # ── Types (specific before generic) ───────────────────────────────────
    ("ul_reply_recv_args_t",      "ulmk_reply_recv_args_t"),
    ("ul_recv_or_notif_result_t", "ulmk_recv_or_notif_result_t"),
    ("ul_thread_attr_t",          "ulmk_thread_attr_t"),
    ("ul_thread_t",               "ulmk_thread_t"),
    ("ul_boot_info_t",            "ulmk_boot_info_t"),
    ("ul_domain_desc_t",          "ulmk_domain_desc_t"),
    ("ul_arch_ctx_t",             "ulmk_arch_ctx_t"),
    ("ul_arch_irq_key_t",         "ulmk_arch_irq_key_t"),
    ("ul_arch_region_t",          "ulmk_arch_region_t"),
    ("ul_irq_handler_t",          "ulmk_irq_handler_t"),
    ("ul_privilege_t",            "ulmk_privilege_t"),
    ("ul_tid_t",                  "ulmk_tid_t"),
    ("ul_ep_t",                   "ulmk_ep_t"),
    ("ul_notif_t",                "ulmk_notif_t"),
    ("ul_msg_t",                  "ulmk_msg_t"),
    ("ul_kern_t",                 "ulmk_kern_t"),

    # ── UL_ARCH_ macros ───────────────────────────────────────────────────
    ("UL_ARCH_QEMU_VIRT_CONSOLE",  "ULMK_ARCH_QEMU_VIRT_CONSOLE"),
    ("UL_ARCH_SRC_STM0_SR0",       "ULMK_ARCH_SRC_STM0_SR0"),
    ("UL_ARCH_SRC_SRE_BIT",        "ULMK_ARCH_SRC_SRE_BIT"),
    ("UL_ARCH_IDLE_IS_WAIT",       "ULMK_ARCH_IDLE_IS_WAIT"),
    ("UL_ARCH_MAX_MPU_REGIONS",    "ULMK_ARCH_MAX_MPU_REGIONS"),
    ("UL_ARCH_NUM_DPR",            "ULMK_ARCH_NUM_DPR"),
    ("UL_ARCH_NUM_CPR",            "ULMK_ARCH_NUM_CPR"),
    ("UL_ARCH_NUM_PRS",            "ULMK_ARCH_NUM_PRS"),
    ("UL_ARCH_STACK_ALIGN",        "ULMK_ARCH_STACK_ALIGN"),
    ("UL_ARCH_MAX_REGIONS",        "ULMK_ARCH_MAX_REGIONS"),
    ("UL_ARCH_",                   "ULMK_ARCH_"),

    # ── Config and kernel build macros ─────────────────────────────────────
    ("UL_CONFIG_DEBUG_PRINTK",     "ULMK_CONFIG_DEBUG_PRINTK"),
    ("UL_CONFIG_",                 "ULMK_CONFIG_"),
    ("UL_KERNEL_BUILD",            "ULMK_KERNEL_BUILD"),

    # ── Permission/cap/mmap/region flags ──────────────────────────────────
    ("UL_PERM_READ",    "ULMK_PERM_READ"),
    ("UL_PERM_WRITE",   "ULMK_PERM_WRITE"),
    ("UL_PERM_EXEC",    "ULMK_PERM_EXEC"),
    ("UL_PERM_USER",    "ULMK_PERM_USER"),
    ("UL_CAP_SPAWN",    "ULMK_CAP_SPAWN"),
    ("UL_CAP_KILL",     "ULMK_CAP_KILL"),
    ("UL_CAP_IRQ",      "ULMK_CAP_IRQ"),
    ("UL_CAP_MAP_PERIPH","ULMK_CAP_MAP_PERIPH"),
    ("UL_CAP_GRANT_CAP","ULMK_CAP_GRANT_CAP"),
    ("UL_CAP_TIMER",    "ULMK_CAP_TIMER"),
    ("UL_CAP_ALL",      "ULMK_CAP_ALL"),
    ("UL_MMAP_ANON",    "ULMK_MMAP_ANON"),
    ("UL_MMAP_PERIPH",  "ULMK_MMAP_PERIPH"),
    ("UL_REGION_CODE",  "ULMK_REGION_CODE"),
    ("UL_REGION_DATA",  "ULMK_REGION_DATA"),
    ("UL_REGION_STACK", "ULMK_REGION_STACK"),
    ("UL_REGION_HEAP",  "ULMK_REGION_HEAP"),
    ("UL_REGION_PERIPH","ULMK_REGION_PERIPH"),
    ("UL_REGION_SHARED","ULMK_REGION_SHARED"),
    ("UL_PRIV_USER",    "ULMK_PRIV_USER"),
    ("UL_PRIV_DRIVER",  "ULMK_PRIV_DRIVER"),
    ("UL_PRIV_KERNEL",  "ULMK_PRIV_KERNEL"),

    # ── Error codes ───────────────────────────────────────────────────────
    ("UL_OK",       "ULMK_OK"),
    ("UL_EINVAL",   "ULMK_EINVAL"),
    ("UL_ENOMEM",   "ULMK_ENOMEM"),
    ("UL_EPERM",    "ULMK_EPERM"),
    ("UL_ENOSPC",   "ULMK_ENOSPC"),
    ("UL_EDEADLK",  "ULMK_EDEADLK"),
    ("UL_ESRCH",    "ULMK_ESRCH"),
    ("UL_ETIMEOUT", "ULMK_ETIMEOUT"),

    # ── Syscall numbers ───────────────────────────────────────────────────
    ("UL_SYS_", "ULMK_SYS_"),
    ("UL_SYSCALL_", "ULMK_SYSCALL_"),

    # ── Handle/sentinel macros ────────────────────────────────────────────
    ("UL_TID_INVALID",          "ULMK_TID_INVALID"),
    ("UL_EP_INVALID",           "ULMK_EP_INVALID"),
    ("UL_NOTIF_INVALID",        "ULMK_NOTIF_INVALID"),
    ("UL_MSG_WORDS",            "ULMK_MSG_WORDS"),
    ("UL_BOOT_MAX_MEM_REGIONS", "ULMK_BOOT_MAX_MEM_REGIONS"),

    # ── Linker / domain macros ────────────────────────────────────────────
    ("UL_DOMAIN_BSS",    "ULMK_DOMAIN_BSS"),
    ("UL_DOMAIN_DATA",   "ULMK_DOMAIN_DATA"),
    ("UL_PRIVATE_INIT",  "ULMK_PRIVATE_INIT"),
    ("UL_PRIVATE",       "ULMK_PRIVATE"),
    ("UL_DEFINE_DOMAIN", "ULMK_DEFINE_DOMAIN"),
    ("UL_MODULE_NAME",   "ULMK_MODULE_NAME"),

    # ── Linker sizing symbols ─────────────────────────────────────────────
    ("UL_MPU_ALIGN",         "ULMK_MPU_ALIGN"),
    ("UL_KERNEL_STACK_SIZE", "ULMK_KERNEL_STACK_SIZE"),
    ("UL_ISR_STACK_SIZE",    "ULMK_ISR_STACK_SIZE"),
    ("UL_CSA_POOL_SIZE",     "ULMK_CSA_POOL_SIZE"),

    # ── Include paths ─────────────────────────────────────────────────────
    ('#include <ul/',            '#include <ulmk/'),
    ('#include "ul/',            '#include "ulmk/'),
    ('#include <ul_arch.h>',     '#include <ulmk_arch.h>'),
    ('#include <ul_syscall_abi.h>', '#include <ulmk_syscall_abi.h>'),
    ('"include/ul/',             '"include/ulmk/'),
    ('include/ul/',              'include/ulmk/'),      # cmake path
    ('$(GEN_INC)/ul',            '$(GEN_INC)/ulmk'),   # Makefile gen path
    ('/ul/config.h',             '/ulmk/config.h'),

    # ── Linker script symbols (_ul_ → _ulmk_) ─────────────────────────────
    ("_ul_trap_table",   "_ulmk_trap_table"),
    ("_ul_int_table",    "_ulmk_int_table"),
    ("_ul_kernel_",      "_ulmk_kernel_"),
    ("_ul_isr_stack",    "_ulmk_isr_stack"),
    ("_ul_csa_pool",     "_ulmk_csa_pool"),
    ("_ul_domain_",      "_ulmk_domain_"),
    ("_ul_user_pool",    "_ulmk_user_pool"),
    ("_ul_small_data",   "_ulmk_small_data"),
    ("_ul_",             "_ulmk_"),           # catch-all

    # ── Include guards ────────────────────────────────────────────────────
    ("UL_MICROKERNEL_H",    "ULMK_MICROKERNEL_H"),
    ("UL_LINKER_H",         "ULMK_LINKER_H"),
    ("UL_SYSCALL_NR_H",     "ULMK_SYSCALL_NR_H"),
    ("UL_SYSCALL_ABI_H",    "ULMK_SYSCALL_ABI_H"),
    ("UL_ARCH_H",           "ULMK_ARCH_H"),
    ("#ifndef ARCH_CONFIG_H", "#ifndef ULMK_ARCH_CONFIG_H"),
    ("#define ARCH_CONFIG_H", "#define ULMK_ARCH_CONFIG_H"),
    ("#endif /* ARCH_CONFIG_H", "#endif /* ULMK_ARCH_CONFIG_H"),

    # ── CMake functions and internal variables ────────────────────────────
    ("ul_component_register",   "ulmk_component_register"),
    ("ul_components_finalize",  "ulmk_components_finalize"),
    ("ul_generate_linker_script","ulmk_generate_linker_script"),
    ("ul_add_app",              "ulmk_add_app"),
    ("ul_add_domain",           "ulmk_add_domain"),
    ("ul_set_root_thread",      "ulmk_set_root_thread"),
    ("ul_finalize_elf",         "ulmk_finalize_elf"),
    ("_UL_COMPONENTS",          "_ULMK_COMPONENTS"),
    ("_UL_ROOT_THREAD_COMP",    "_ULMK_ROOT_THREAD_COMP"),
    ("_UL_BOARD_SRCS",          "_ULMK_BOARD_SRCS"),
    ("UL_COMP_",                "ULMK_COMP_"),
    ("UL_BOARD_CPU",            "ULMK_BOARD_CPU"),
    ("UL_BOARD_CFLAGS",         "ULMK_BOARD_CFLAGS"),
    ("UL_BOARD_SOURCES",        "ULMK_BOARD_SOURCES"),
    ("UL_CHIP_DIR",             "ULMK_CHIP_DIR"),
    ("UL_HAS_APPS",             "ULMK_HAS_APPS"),
    ("UL_APP_LIST",             "ULMK_APP_LIST"),
    ("UL_DOMAIN_LIST",          "ULMK_DOMAIN_LIST"),
    ("UL_APP_NAME",             "ULMK_APP_NAME"),
    ("ulipe_kernel",            "ulmk_kernel"),
    ("ulipe_microkernel",       "ulmk"),

    # ── Project name strings ──────────────────────────────────────────────
    ("ulipeMicroKernel:",  "ulmk:"),
    ("ulipeMicroKernel",   "ulmk"),
    ("ulipemicrokernel",   "ulmk"),

    # ── Catch remaining ul_ identifiers ──────────────────────────────────
    ("ul_",  "ulmk_"),
]

# --- Files/dirs to skip completely ----------------------------------------

SKIP_FILENAMES = {
    "microkernel_book_tricore.md",
    "tricore_guide_pt.md",
    "microkernel_book.md",
    "rename_ulmk.py",       # this script itself
}

SKIP_DIRS = {
    ".git",
    "build",
    "__pycache__",
}

# --- File extensions to process -------------------------------------------

PROCESS_EXTS = {
    ".c", ".h", ".S", ".s",
    ".cmake", ".txt",       # CMakeLists.txt, *.cmake
    ".ld", ".in",           # *.ld, *.ld.in
    ".py",
    ".md", ".mdc",
    "",                     # Makefile (no extension)
}

def should_process(path):
    basename = os.path.basename(path)
    if basename in SKIP_FILENAMES:
        return False
    _, ext = os.path.splitext(basename)
    return ext in PROCESS_EXTS

def process_file(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            original = f.read()
    except Exception as e:
        print(f"  SKIP (read error): {path}: {e}")
        return

    content = original
    for old, new in REPLACEMENTS:
        content = content.replace(old, new)

    if content != original:
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"  updated: {path}")

def walk(root):
    for dirpath, dirnames, filenames in os.walk(root):
        # Prune skip dirs
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fname in filenames:
            fpath = os.path.join(dirpath, fname)
            if should_process(fpath):
                process_file(fpath)

if __name__ == "__main__":
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    print(f"Repo root: {repo}")
    walk(repo)
    print("Done.")

#!/usr/bin/env python3
"""Surgical backport of standalone-boot fixes onto 0.1.x tags."""
import re
import subprocess
import textwrap

K = "/home/ulipe/fun/ulmk"
B = "/home/ulipe/fun/ulmk_boards"
LOG = []


def run(cwd, *args, check=True):
	r = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
	if check and r.returncode != 0:
		raise SystemExit(
			f"FAIL {' '.join(args)}\n{r.stdout}\n{r.stderr}")
	return r


def log(m):
	LOG.append(m)
	print(m)


def write(path, content):
	with open(path, "w", encoding="utf-8") as f:
		f.write(content)


def read(path):
	with open(path, encoding="utf-8") as f:
		return f.read()


def patch_arch_config(path):
	s = read(path)
	old = """#define ULMK_ARCH_CSFR_CPRE_0	0xE000u
#define ULMK_ARCH_CSFR_CPRE_1	0xE004u
#define ULMK_ARCH_CSFR_CPRE_2	0xE008u
#define ULMK_ARCH_CSFR_CPRE_3	0xE00Cu
#define ULMK_ARCH_CSFR_DPRE_0	0xE010u
#define ULMK_ARCH_CSFR_DPRE_1	0xE014u
#define ULMK_ARCH_CSFR_DPRE_2	0xE018u
#define ULMK_ARCH_CSFR_DPRE_3	0xE01Cu
#define ULMK_ARCH_CSFR_DPWE_0	0xE020u
#define ULMK_ARCH_CSFR_DPWE_1	0xE024u
#define ULMK_ARCH_CSFR_DPWE_2	0xE028u
#define ULMK_ARCH_CSFR_DPWE_3	0xE02Cu
#define ULMK_ARCH_CSFR_CPXE_0	0xE040u
#define ULMK_ARCH_CSFR_CPXE_1	0xE044u
#define ULMK_ARCH_CSFR_CPXE_2	0xE048u
#define ULMK_ARCH_CSFR_CPXE_3	0xE04Cu
"""
	new = """/*
 * TC2xx/TC3xx MPU enables (UM): CPXE @ E000, DPRE @ E010, DPWE @ E020.
 * There is no CPRE register.  Older internal docs listed CPXE @ E040; on
 * TC27x that address is Safety RGNLA — never program it as CPXE.
 */
#define ULMK_ARCH_CSFR_CPXE_0	0xE000u
#define ULMK_ARCH_CSFR_CPXE_1	0xE004u
#define ULMK_ARCH_CSFR_CPXE_2	0xE008u
#define ULMK_ARCH_CSFR_CPXE_3	0xE00Cu
#define ULMK_ARCH_CSFR_DPRE_0	0xE010u
#define ULMK_ARCH_CSFR_DPRE_1	0xE014u
#define ULMK_ARCH_CSFR_DPRE_2	0xE018u
#define ULMK_ARCH_CSFR_DPRE_3	0xE01Cu
#define ULMK_ARCH_CSFR_DPWE_0	0xE020u
#define ULMK_ARCH_CSFR_DPWE_1	0xE024u
#define ULMK_ARCH_CSFR_DPWE_2	0xE028u
#define ULMK_ARCH_CSFR_DPWE_3	0xE02Cu
"""
	if old not in s:
		raise SystemExit(f"arch_config pattern missing in {path}")
	write(path, s.replace(old, new))


def patch_arch_c(path):
	s = read(path)
	# trampoline → user_entry in ctx_init
	if "extern void _ulmk_thread_trampoline(void);" in s:
		s = s.replace(
			"extern void _ulmk_thread_trampoline(void);",
			"extern void ulmk_user_thread_entry(void (*entry)(void *arg), void *arg);")
	# replace trampoline PC assignments
	s = s.replace(
		"(uint32_t)(uintptr_t)_ulmk_thread_trampoline",
		"(uint32_t)(uintptr_t)ulmk_user_thread_entry")
	# If A14=entry and A4=arg still (old layout), also set A4=entry A5=arg
	# Match lower_csa[8] = arg and add A5; remove A14 entry if present
	if "upper_csa[10] = (uint32_t)(uintptr_t)entry;" in s:
		s = s.replace(
			"upper_csa[10] = (uint32_t)(uintptr_t)entry;\t/* A14: entry fn */\n",
			"")
		s = s.replace(
			"upper_csa[10] = (uint32_t)(uintptr_t)entry;\n",
			"")
	if "lower_csa[8] = (uint32_t)(uintptr_t)arg;" in s and \
	   "lower_csa[9]" not in s:
		s = s.replace(
			"lower_csa[8] = (uint32_t)(uintptr_t)arg;\t/* A4: pointer arg */",
			"lower_csa[8] = (uint32_t)(uintptr_t)entry;\t/* A4 */\n"
			"\tlower_csa[9] = (uint32_t)(uintptr_t)arg;\t/* A5 */")
		s = s.replace(
			"lower_csa[8] = (uint32_t)(uintptr_t)arg;",
			"lower_csa[8] = (uint32_t)(uintptr_t)entry;\n"
			"\tlower_csa[9] = (uint32_t)(uintptr_t)arg;")

	# mpu_write_enables: drop CPRE write
	old_en = """static void mpu_write_enables(uint8_t prs, uint32_t dpre, uint32_t dpwe,
			      uint32_t cpre, uint32_t cpxe)
{
	mpu_mtcr(ULMK_ARCH_CSFR_DPRE_0 + (uint32_t)prs * 4u, dpre);
	mpu_mtcr(ULMK_ARCH_CSFR_DPWE_0 + (uint32_t)prs * 4u, dpwe);
	mpu_mtcr(ULMK_ARCH_CSFR_CPRE_0 + (uint32_t)prs * 4u, cpre);
	mpu_mtcr(ULMK_ARCH_CSFR_CPXE_0 + (uint32_t)prs * 4u, cpxe);
	__asm__ volatile("isync" ::: "memory");
}"""
	new_en = """static void mpu_write_enables(uint8_t prs, uint32_t dpre, uint32_t dpwe,
			      uint32_t cpxe)
{
	mpu_mtcr(ULMK_ARCH_CSFR_DPRE_0 + (uint32_t)prs * 4u, dpre);
	mpu_mtcr(ULMK_ARCH_CSFR_DPWE_0 + (uint32_t)prs * 4u, dpwe);
	mpu_mtcr(ULMK_ARCH_CSFR_CPXE_0 + (uint32_t)prs * 4u, cpxe);
	__asm__ volatile("isync" ::: "memory");
}"""
	if old_en not in s:
		raise SystemExit("mpu_write_enables pattern missing")
	s = s.replace(old_en, new_en)

	# Fix all 5-arg call sites → 4-arg (drop duplicate cpre)
	# mpu_write_enables(x, a, b, c, c) → mpu_write_enables(x, a, b, c)
	s = re.sub(
		r"mpu_write_enables\(([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*\4\)",
		r"mpu_write_enables(\1, \2, \3, \4)",
		s)
	# mpu_write_enables(i, 0, 0, 0, 0)
	s = s.replace("mpu_write_enables(i, 0u, 0u, 0u, 0u)",
		      "mpu_write_enables(i, 0u, 0u, 0u)")
	# mpu_write_enables(prs, dpre, dpwe, cpre, cpxe)
	s = s.replace("mpu_write_enables(prs, dpre, dpwe, cpre, cpxe)",
		      "mpu_write_enables(prs, dpre, dpwe, cpxe)")
	# prs1_cpre usages in mpu_init — simplify if both vars exist
	s = s.replace("prs1_cpre,\n\t\t\t  prs1_cpxe)", "prs1_cpxe)")
	s = re.sub(r"uint32_t\s+prs1_cpre;\n\s*", "", s)
	s = re.sub(r"prs1_cpre = 0u;\n\s*", "", s)
	s = re.sub(
		r"prs1_cpre = \(1u << ULMK_ARCH_MPU_CPR_USER\);\n\s*",
		"", s)
	# mpu_prs1_static_enables signature
	s = s.replace(
		"static void mpu_prs1_static_enables(uint32_t *dpre, uint32_t *dpwe,\n"
		"\t\t\t\t    uint32_t *cpre, uint32_t *cpxe)",
		"static void mpu_prs1_static_enables(uint32_t *dpre, uint32_t *dpwe,\n"
		"\t\t\t\t    uint32_t *cpxe)")
	s = s.replace("mpu_prs1_static_enables(&dpre, &dpwe, &cpre, &cpxe)",
		      "mpu_prs1_static_enables(&dpre, &dpwe, &cpxe)")
	s = s.replace("\t*cpre = 0u;\n\t*cpxe = 0u;\n", "\t*cpxe = 0u;\n")
	s = s.replace(
		"\t\t*cpre = (1u << ULMK_ARCH_MPU_CPR_USER);\n"
		"\t\t*cpxe = (1u << ULMK_ARCH_MPU_CPR_USER);\n",
		"\t\t*cpxe = (1u << ULMK_ARCH_MPU_CPR_USER);\n")
	# drop unused cpre locals in mpu_program_regions if present
	s = re.sub(r"\tuint32_t cpre;\n(\tuint32_t cpxe;\n)", r"\1", s)

	# mpu_range_upper: end-8 → exclusive
	s = s.replace(
		"return (uint32_t)end - 8u;",
		"return (uint32_t)end & ~7u;")
	s = s.replace(
		"return ((uint32_t)end - 1u) & ~7u;",
		"return (uint32_t)end & ~7u;")

	if "ULMK_ARCH_CSFR_CPRE" in s:
		raise SystemExit("still has CPRE refs")
	write(path, s)


def patch_ctx_switch(path):
	# Take main's version of comments + drop trampoline — file is small
	main = subprocess.check_output(
		["git", "show", "main:arch/tricore/ctx_switch.S"],
		cwd=K, text=True)
	write(path, main)


def patch_kernel_text(path):
	s = read(path)
	if "ALIGN(ULMK_MPU_ALIGN)" in s and "_ulmk_kernel_text_end" in s:
		return
	# insert ALIGN before end symbols
	s = s.replace(
		"\t_ulmk_kernel_text_end = .;",
		"\t. = ALIGN(ULMK_MPU_ALIGN);\n"
		"\t_ulmk_kernel_text_end = .;")
	write(path, s)


def patch_startup(path):
	main = subprocess.check_output(
		["git", "show", "main:arch/tricore/startup.S"],
		cwd=K, text=True)
	# If 0.1.x startup is close enough, use the early-WDT+IS hunk from main
	# by copying main wholesale only if structure matches
	cur = read(path)
	if "ulmk_board_wdt_disable_early" in cur:
		return
	# Insert after disable
	needle = "\tdisable\n\n\t/* 1. Supervisor stack */"
	repl = textwrap.dedent("""\
		disable

		/*
		 * Board hook: disable CPU0 + Safety WDT before CSA construction.
		 */
		movh.a  %a11, hi:_start_after_wdt
		lea     %a11, [%a11] lo:_start_after_wdt
		movh.a  %a2, hi:ulmk_board_wdt_disable_early
		lea     %a2, [%a2] lo:ulmk_board_wdt_disable_early
		ji      %a2
		.global _start_after_wdt
	_start_after_wdt:

		/* 1. Supervisor stack + clear PSW.IS */
	""")
	if needle not in cur:
		# try alternate
		needle = "    disable\n\n    /* 1. Supervisor stack */"
		repl = repl.replace("\t", "    ")
	if needle not in cur:
		raise SystemExit("startup needle missing:\n" + cur[:400])
	cur = cur.replace(needle, repl)
	# also clear IS after stack set
	stack = """    movh.a  %a10, hi:_ulmk_kernel_stack_top
    lea     %a10, [%a10] lo:_ulmk_kernel_stack_top
"""
	stack_tabs = stack.replace("    ", "\t")
	is_clear = """    movh.a  %a10, hi:_ulmk_kernel_stack_top
    lea     %a10, [%a10] lo:_ulmk_kernel_stack_top
    mfcr    %d0, 0xFE04
    mov     %d1, 0x200
    andn    %d0, %d0, %d1
    mtcr    0xFE04, %d0
    isync
"""
	if stack in cur:
		cur = cur.replace(stack, is_clear)
	elif stack_tabs in cur:
		cur = cur.replace(stack_tabs, is_clear.replace("    ", "\t"))
	if ".extern ulmk_board_wdt_disable_early" not in cur:
		cur = cur.replace(
			".extern ulmk_kern_start\n",
			".extern ulmk_kern_start\n"
			"    .extern ulmk_board_wdt_disable_early\n")
	write(path, cur)


def ensure_wdt_stub_and_cmake():
	stub = K + "/arch/tricore/board_wdt_early_stub.S"
	write(stub, textwrap.dedent("""\
		/* SPDX-License-Identifier: MIT */
		/*
		 * Weak no-op — boards override with board_wdt_early.S (ji %a11 return).
		 */

			.section .text.board_wdt_early, "ax"
			.weak   ulmk_board_wdt_disable_early
			.type   ulmk_board_wdt_disable_early, @function
		ulmk_board_wdt_disable_early:
			ji      %a11
			.size   ulmk_board_wdt_disable_early, . - ulmk_board_wdt_disable_early
		"""))
	cmake = K + "/cmake/arch_sources.cmake"
	s = read(cmake)
	if "board_wdt_early_stub.S" not in s:
		s = s.replace(
			"${ULMK_ARCH_DIR}/startup.S\n",
			"${ULMK_ARCH_DIR}/startup.S\n"
			"\t\t${ULMK_ARCH_DIR}/board_wdt_early_stub.S\n")
		write(cmake, s)
	# ulmk_arch.h endinit decls
	h = K + "/arch/tricore/include/ulmk_arch.h"
	s = read(h)
	if "ulmk_board_cpu_endinit_clear" not in s:
		s = s.replace(
			"void ulmk_board_init(void);\n",
			"void ulmk_board_init(void);\n"
			"\n"
			"void ulmk_board_cpu_endinit_clear(void);\n"
			"void ulmk_board_cpu_endinit_set(void);\n")
		write(h, s)


def backport_ulmk_tag(tag):
	log(f"\n=== ulmk {tag} surgical ===")
	main = run(K, "git", "rev-parse", "main").stdout.strip()
	br = f"backport/{tag}"
	run(K, "git", "branch", "-D", br, check=False)
	run(K, "git", "checkout", "-b", br, tag)
	try:
		patch_arch_config(K + "/arch/tricore/include/arch_config.h")
		patch_arch_c(K + "/arch/tricore/arch.c")
		patch_ctx_switch(K + "/arch/tricore/ctx_switch.S")
		patch_kernel_text(K + "/linker/kernel/kernel_text.ld.in")
		patch_startup(K + "/arch/tricore/startup.S")
		ensure_wdt_stub_and_cmake()
		run(K, "git", "add", "-A")
		msg = textwrap.dedent("""\
			backport: standalone boot — MPX entry, CPXE map, early WDT

			Cherry-picked surgically from main for this release tag.
			""")
		run(K, "git", "commit", "-m", msg)
		run(K, "git", "tag", "-f", tag)
		r = run(K, "git", "push", "origin", f"refs/tags/{tag}",
			"--force", check=False)
		log(f"push {tag} rc={r.returncode}")
		if r.returncode != 0:
			log(r.stderr)
	finally:
		run(K, "git", "checkout", "main")
		run(K, "git", "branch", "-D", br, check=False)
		run(K, "git", "reset", "--hard", main)


def backport_boards_010():
	log("\n=== ulmk_boards 0.1.0 surgical ===")
	main = run(B, "git", "rev-parse", "main").stdout.strip()
	br = "backport/0.1.0"
	run(B, "git", "branch", "-D", br, check=False)
	run(B, "git", "checkout", "-b", br, "0.1.0")
	try:
		# Take main versions of files that exist; skip board_smp.h
		files = [
			"tc275_lite/bmhd.c",
			"tc275_lite/bmhd.ld.in",
			"tc275_lite/tools/gen_bmhd_crc.py",
			"tc275_lite/board.cmake",
			"tc275_lite/board_init.c",
			"tc275_lite/board_regs.h",
			"tc275_lite/board_wdt_early.S",
			"tc275_lite/openocd/patches/0003-jtag-tas-client-sole-target.patch",
			"tc275_lite/openocd/tc275_lite_hil.cfg",
			"tc275_lite/openocd/tc275_lite_hotattach.cfg",
			"tc275_lite/scripts/debug.sh",
			"tc275_lite/scripts/flash.sh",
			"tc275_lite/scripts/hil-config.sh",
			"tc275_lite/scripts/hil-release-run.sh",
			"tc275_lite/scripts/hil-reset-cause.sh",
		]
		for f in files:
			blob = subprocess.check_output(
				["git", "show", f"main:{f}"], cwd=B)
			path = f"{B}/{f}"
			import os
			os.makedirs(os.path.dirname(path), exist_ok=True)
			with open(path, "wb") as out:
				out.write(blob)
		# board_init.c on main includes board_smp.h decls — if 0.1.0 has no
		# board_smp.h, strip the include if present and keep functions.
		bi = read(B + "/tc275_lite/board_init.c")
		# Ensure board_smp.h include isn't required — decls are in .c
		# Check if board_init references board_smp.h
		# main board_init doesn't include board_smp.h (decls are there)
		run(B, "git", "add", "-A")
		# drop board_smp.h if it appeared from cherry-pick residue
		import os
		smp = B + "/tc275_lite/board_smp.h"
		if os.path.exists(smp):
			# 0.1.0 didn't have it — only add if something includes it
			# Keep a minimal header with the new decls
			write(smp, textwrap.dedent("""\
				/* SPDX-License-Identifier: MIT */
				#ifndef ULMK_BOARD_SMP_H
				#define ULMK_BOARD_SMP_H

				#include <stdint.h>

				void ulmk_board_init_extra_periphs(void);
				void ulmk_board_i2c0_hw_init(void);

				#endif
				"""))
			run(B, "git", "add", smp)
		msg = textwrap.dedent("""\
			backport: tc275_lite standalone boot after button/PORST

			Surgical backport from main for the 0.1.0 tag (no SMP board).
			""")
		run(B, "git", "commit", "-m", msg)
		run(B, "git", "tag", "-f", "0.1.0")
		r = run(B, "git", "push", "origin", "refs/tags/0.1.0",
			"--force", check=False)
		log(f"push boards 0.1.0 rc={r.returncode}")
		if r.returncode != 0:
			log(r.stderr)
	finally:
		run(B, "git", "checkout", "main")
		run(B, "git", "branch", "-D", br, check=False)
		run(B, "git", "reset", "--hard", main)


if __name__ == "__main__":
	import subprocess
	for tag in ["0.1.0", "0.1.1"]:
		backport_ulmk_tag(tag)
	backport_boards_010()
	open("/tmp/backport2.txt", "w").write("\n".join(LOG))
	print("ALL DONE")

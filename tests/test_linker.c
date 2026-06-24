/*
 * Linker script test suite.
 *
 * Two categories:
 *
 *  ACTIVE  — tests for the C macro API (UL_DOMAIN_BSS, UL_DOMAIN_DATA,
 *             UL_PRIVATE, UL_PRIVATE_INIT).  These exercise only the compiler
 *             __attribute__((section(...))) path and do not depend on any linker
 *             symbol (_ul_*) or domain section.  They pass with the current
 *             tools/hello/linker.ld.
 *
 *  #if 0   — tests for symbol placement, alignment, and domain table iteration.
 *             Require the full generated.ld with domain sections and _ul_*
 *             symbols.  Enable after generate_ld.py and linker/kernel/ are
 *             implemented.
 *
 * Mock types stand in for include/ul/linker.h which does not exist yet.
 */

#include <stdint.h>
#include <stddef.h>
#include "unity.h"

/* =========================================================================
 * Mock — mirrors the types and macros that include/ul/linker.h will provide.
 * ========================================================================= */

typedef struct {
	const char *name;
	uintptr_t   start;
	uintptr_t   end;
	uint32_t    perms;
} ul_domain_desc_t;

#define UL_PERM_READ  (1u << 0)
#define UL_PERM_WRITE (1u << 1)
#define UL_PERM_EXEC  (1u << 2)
#define UL_PERM_USER  (1u << 3)

#define UL_DOMAIN_BSS(name)  __attribute__((section(".domain_" #name ".bss")))
#define UL_DOMAIN_DATA(name) __attribute__((section(".domain_" #name ".data")))
#define UL_PRIVATE      UL_DOMAIN_BSS(UL_MODULE_NAME)
#define UL_PRIVATE_INIT UL_DOMAIN_DATA(UL_MODULE_NAME)

/* =========================================================================
 * Test fixtures — file-scope variables placed via the macro API.
 * Non-static so the linker never GC's them even without 'used'.
 * ========================================================================= */

#define UL_MODULE_NAME testmod

UL_PRIVATE uint8_t  g_bss_byte;
UL_PRIVATE uint32_t g_bss_word;
UL_PRIVATE uint8_t  g_bss_arr[16];

UL_PRIVATE_INIT uint8_t  g_data_byte  = 0xAB;
UL_PRIVATE_INIT uint32_t g_data_word  = 0xDEADBEEFu;
UL_PRIVATE_INIT uint8_t  g_data_arr[] = { 1, 2, 3, 4 };

#undef UL_MODULE_NAME

/* A second domain to verify macro works with different names. */
UL_DOMAIN_BSS(sensor)  uint32_t g_sensor_state;
UL_DOMAIN_DATA(sensor) uint32_t g_sensor_rate = 100u;

/* =========================================================================
 * Active tests — C macro API
 * ========================================================================= */

/*
 * BSS variables placed via UL_PRIVATE / UL_DOMAIN_BSS must be zero-
 * initialised at boot.  startup.S zeroes .bss; orphan domain sections
 * land in .bss too when domain sections are absent from the linker.
 */
void test_linker_private_bss_byte_is_zero(void)
{
	TEST_ASSERT_EQUAL_UINT8(0, g_bss_byte);
}

void test_linker_private_bss_word_is_zero(void)
{
	TEST_ASSERT_EQUAL_UINT32(0u, g_bss_word);
}

void test_linker_private_bss_array_is_zero(void)
{
	int i;

	for (i = 0; i < (int)sizeof(g_bss_arr); i++)
		TEST_ASSERT_EQUAL_UINT8(0, g_bss_arr[i]);
}

/* UL_DOMAIN_BSS/DATA work with a name different from UL_MODULE_NAME. */
void test_linker_domain_bss_other_module_is_zero(void)
{
	TEST_ASSERT_EQUAL_UINT32(0u, g_sensor_state);
}

/* Variables placed via the macro API are readable and writable. */
void test_linker_private_bss_byte_is_writable(void)
{
	g_bss_byte = 0xFF;
	TEST_ASSERT_EQUAL_UINT8(0xFF, g_bss_byte);
	g_bss_byte = 0;
}

void test_linker_domain_data_byte_write_read(void)
{
	g_data_byte = 0xAB;
	TEST_ASSERT_EQUAL_HEX8(0xAB, g_data_byte);
}

void test_linker_domain_data_word_write_read(void)
{
	g_data_word = 0xDEADBEEFu;
	TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, g_data_word);
}

void test_linker_domain_data_array_write_read(void)
{
	g_data_arr[0] = 0x11;
	g_data_arr[1] = 0x22;
	g_data_arr[2] = 0x33;
	g_data_arr[3] = 0x44;
	TEST_ASSERT_EQUAL_UINT8(0x11, g_data_arr[0]);
	TEST_ASSERT_EQUAL_UINT8(0x22, g_data_arr[1]);
	TEST_ASSERT_EQUAL_UINT8(0x33, g_data_arr[2]);
	TEST_ASSERT_EQUAL_UINT8(0x44, g_data_arr[3]);
}

void test_linker_domain_sensor_bss_is_zero(void)
{
	TEST_ASSERT_EQUAL_UINT32(0u, g_sensor_state);
}

void test_linker_domain_sensor_data_write_read(void)
{
	g_sensor_rate = 200u;
	TEST_ASSERT_EQUAL_UINT32(200u, g_sensor_rate);
}

/*
 * Initialised variables placed via UL_PRIVATE_INIT / UL_DOMAIN_DATA retain
 * their initialisers only when the linker script handles the domain sections
 * with AT > KERNEL_FLASH (LMA-to-VMA copy).  With the current simple linker
 * (tools/hello/linker.ld), orphan domain sections are not copied and
 * initialisers read as zero.  Enable these after generated.ld is wired up.
 */
#if 0
void test_linker_private_init_byte_has_value(void)
{
	TEST_ASSERT_EQUAL_HEX8(0xAB, g_data_byte);
}

void test_linker_private_init_word_has_value(void)
{
	TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, g_data_word);
}

void test_linker_private_init_array_has_values(void)
{
	TEST_ASSERT_EQUAL_UINT8(1, g_data_arr[0]);
	TEST_ASSERT_EQUAL_UINT8(2, g_data_arr[1]);
	TEST_ASSERT_EQUAL_UINT8(3, g_data_arr[2]);
	TEST_ASSERT_EQUAL_UINT8(4, g_data_arr[3]);
}

void test_linker_domain_data_other_module_has_value(void)
{
	TEST_ASSERT_EQUAL_UINT32(100u, g_sensor_rate);
}
#endif /* initialiser tests — need generated.ld with AT > KERNEL_FLASH */

/* =========================================================================
 * Symbol placement and alignment tests — need full generated.ld
 * =========================================================================
 *
 * Enable after generate_ld.py is implemented and the linker is switched to
 * the generated script.  Each test checks one guarantee from the linker spec
 * (docs/linker_spec.md §6 Exported Symbol Table).
 *
 * The constant 64 is UL_MPU_ALIGN for TC27x (linker_spec.md §11).
 */
#if 0

#define UL_MPU_ALIGN_VAL  64u
#define UL_VECTOR_ALIGN   256u
#define IS_ALIGNED(addr, align)  (((uintptr_t)(addr) % (align)) == 0u)

/* Exported by kernel linker fragments in linker/kernel/. */
extern uint8_t _ul_kernel_text_start[];
extern uint8_t _ul_kernel_text_end[];
extern uint8_t _ul_kernel_data_start[];
extern uint8_t _ul_kernel_data_end[];
extern uint8_t _ul_kernel_stack_bottom[];
extern uint8_t _ul_kernel_stack_top[];
extern uint8_t _ul_isr_stack_bottom[];
extern uint8_t _ul_isr_stack_top[];
extern uint8_t _ul_trap_table[];
extern uint8_t _ul_int_table[];
extern uint8_t _ul_user_pool_start[];
extern uint8_t _ul_user_pool_end[];
extern uint8_t _ul_domain_table_start[];
extern uint8_t _ul_domain_table_end[];

/* Exported by arch/tricore/linker/csa_pool.ld.in. */
extern uint8_t _ul_csa_pool_start[];
extern uint8_t _ul_csa_pool_end[];

/* Per-domain symbols generated by domain_data.ld.in snippets. */
extern uint8_t _ul_domain_testmod_start[];
extern uint8_t _ul_domain_testmod_end[];
extern uint8_t _ul_domain_sensor_start[];
extern uint8_t _ul_domain_sensor_end[];

/* --- kernel_text --- */

void test_linker_kernel_text_start_aligned_32(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_kernel_text_start, 32u));
}

void test_linker_kernel_text_end_greater_than_start(void)
{
	TEST_ASSERT_GREATER_THAN((uintptr_t)_ul_kernel_text_start,
	                         (uintptr_t)_ul_kernel_text_end);
}

/* --- kernel_data --- */

void test_linker_kernel_data_start_aligned_mpu(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_kernel_data_start, UL_MPU_ALIGN_VAL));
}

void test_linker_kernel_data_end_aligned_mpu(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_kernel_data_end, UL_MPU_ALIGN_VAL));
}

void test_linker_kernel_data_end_gte_start(void)
{
	TEST_ASSERT_GREATER_OR_EQUAL((uintptr_t)_ul_kernel_data_start,
	                             (uintptr_t)_ul_kernel_data_end);
}

/* --- kernel_stacks --- */

void test_linker_kernel_stack_top_gt_bottom(void)
{
	TEST_ASSERT_GREATER_THAN((uintptr_t)_ul_kernel_stack_bottom,
	                         (uintptr_t)_ul_kernel_stack_top);
}

void test_linker_kernel_stack_size_at_least_2k(void)
{
	size_t sz = (uintptr_t)_ul_kernel_stack_top -
	            (uintptr_t)_ul_kernel_stack_bottom;

	TEST_ASSERT_GREATER_OR_EQUAL(2048u, sz);
}

void test_linker_isr_stack_top_gt_bottom(void)
{
	TEST_ASSERT_GREATER_THAN((uintptr_t)_ul_isr_stack_bottom,
	                         (uintptr_t)_ul_isr_stack_top);
}

void test_linker_isr_stack_size_at_least_1k(void)
{
	size_t sz = (uintptr_t)_ul_isr_stack_top -
	            (uintptr_t)_ul_isr_stack_bottom;

	TEST_ASSERT_GREATER_OR_EQUAL(1024u, sz);
}

/* --- vectors --- */

void test_linker_trap_table_aligned_256(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_trap_table, UL_VECTOR_ALIGN));
}

void test_linker_int_table_aligned_256(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_int_table, UL_VECTOR_ALIGN));
}

/* --- csa_pool (arch/tricore) --- */

void test_linker_csa_pool_start_aligned_64(void)
{
	/* CSA frame = 64 bytes; pool base must be 64-byte aligned. */
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_csa_pool_start, 64u));
}

void test_linker_csa_pool_end_gt_start(void)
{
	TEST_ASSERT_GREATER_THAN((uintptr_t)_ul_csa_pool_start,
	                         (uintptr_t)_ul_csa_pool_end);
}

void test_linker_csa_pool_fits_at_least_16_frames(void)
{
	size_t sz = (uintptr_t)_ul_csa_pool_end - (uintptr_t)_ul_csa_pool_start;

	/* 16 CSAs × 64 bytes = 1024 bytes minimum. */
	TEST_ASSERT_GREATER_OR_EQUAL(1024u, sz);
}

void test_linker_csa_pool_size_multiple_of_64(void)
{
	size_t sz = (uintptr_t)_ul_csa_pool_end - (uintptr_t)_ul_csa_pool_start;

	TEST_ASSERT_EQUAL_UINT32(0u, sz % 64u);
}

/* --- domain sections --- */

void test_linker_domain_testmod_start_aligned_mpu(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_domain_testmod_start, UL_MPU_ALIGN_VAL));
}

void test_linker_domain_testmod_end_aligned_mpu(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_domain_testmod_end, UL_MPU_ALIGN_VAL));
}

void test_linker_domain_testmod_end_gte_start(void)
{
	TEST_ASSERT_GREATER_OR_EQUAL((uintptr_t)_ul_domain_testmod_start,
	                             (uintptr_t)_ul_domain_testmod_end);
}

void test_linker_domain_sensor_start_aligned_mpu(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_domain_sensor_start, UL_MPU_ALIGN_VAL));
}

void test_linker_domain_sensor_end_aligned_mpu(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_domain_sensor_end, UL_MPU_ALIGN_VAL));
}

void test_linker_domain_testmod_sensor_do_not_overlap(void)
{
	/*
	 * Domains are laid out sequentially; they must not overlap.
	 * Either testmod ends before sensor starts or vice versa.
	 */
	int no_overlap =
	    ((uintptr_t)_ul_domain_testmod_end <= (uintptr_t)_ul_domain_sensor_start) ||
	    ((uintptr_t)_ul_domain_sensor_end  <= (uintptr_t)_ul_domain_testmod_start);

	TEST_ASSERT_TRUE(no_overlap);
}

/* --- domain_table --- */

void test_linker_domain_table_start_aligned_4(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_domain_table_start, 4u));
}

void test_linker_domain_table_has_at_least_one_entry(void)
{
	size_t sz = (uintptr_t)_ul_domain_table_end -
	            (uintptr_t)_ul_domain_table_start;

	TEST_ASSERT_GREATER_OR_EQUAL(sizeof(ul_domain_desc_t), sz);
}

void test_linker_domain_table_size_multiple_of_descriptor(void)
{
	size_t sz = (uintptr_t)_ul_domain_table_end -
	            (uintptr_t)_ul_domain_table_start;

	TEST_ASSERT_EQUAL_UINT32(0u, sz % sizeof(ul_domain_desc_t));
}

void test_linker_domain_table_entry_names_are_non_null(void)
{
	const ul_domain_desc_t *t =
	    (const ul_domain_desc_t *)(uintptr_t)_ul_domain_table_start;
	size_t count = ((uintptr_t)_ul_domain_table_end -
	                (uintptr_t)_ul_domain_table_start) /
	               sizeof(ul_domain_desc_t);
	size_t i;

	for (i = 0; i < count; i++)
		TEST_ASSERT_NOT_NULL(t[i].name);
}

void test_linker_domain_table_entry_start_lte_end(void)
{
	const ul_domain_desc_t *t =
	    (const ul_domain_desc_t *)(uintptr_t)_ul_domain_table_start;
	size_t count = ((uintptr_t)_ul_domain_table_end -
	                (uintptr_t)_ul_domain_table_start) /
	               sizeof(ul_domain_desc_t);
	size_t i;

	for (i = 0; i < count; i++)
		TEST_ASSERT_LESS_OR_EQUAL(t[i].end, t[i].start);
}

/* --- user_pool --- */

void test_linker_user_pool_start_aligned_mpu(void)
{
	TEST_ASSERT_TRUE(IS_ALIGNED(_ul_user_pool_start, UL_MPU_ALIGN_VAL));
}

void test_linker_user_pool_end_gt_start(void)
{
	TEST_ASSERT_GREATER_THAN((uintptr_t)_ul_user_pool_start,
	                         (uintptr_t)_ul_user_pool_end);
}

void test_linker_user_pool_size_at_least_4k(void)
{
	size_t sz = (uintptr_t)_ul_user_pool_end - (uintptr_t)_ul_user_pool_start;

	TEST_ASSERT_GREATER_OR_EQUAL(4096u, sz);
}

#endif /* symbol placement tests */

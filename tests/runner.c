/*
 * Test runner — entry point for the ulipeMicroKernel test suite on QEMU.
 *
 * startup.S calls main(); its return value is forwarded to _exit(), which
 * writes to the VIRT device and exits QEMU.  Unity's UNITY_END() returns
 * the number of failed tests, so QEMU exits 0 only when all pass.
 *
 * Enabling a test suite:
 *   1. Remove the #if 0 / #endif block in the matching test_<module>.c.
 *   2. Uncomment the extern declarations and RUN_TEST() calls below.
 *   3. Rebuild with `make` and run with `make run`.
 */

#include "unity.h"

/* setUp/tearDown run before/after every RUN_TEST(). */
void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * test_dummy.c — always active
 * ========================================================================= */
extern void test_dummy_one_plus_one(void);
extern void test_dummy_qemu_output(void);

/* =========================================================================
 * test_thread_api.c
 * ========================================================================= */
/*
extern void test_thread_self_returns_valid_tid(void);
extern void test_thread_self_is_stable(void);
extern void test_thread_self_is_non_negative(void);
extern void test_thread_yield_returns(void);
extern void test_thread_yield_multiple_times_safe(void);
extern void test_thread_priority_get_self_in_range(void);
extern void test_thread_priority_set_and_get_roundtrip(void);
extern void test_thread_priority_set_min(void);
extern void test_thread_priority_set_max(void);
extern void test_thread_priority_set_same_value_twice(void);
extern void test_thread_priority_get_invalid_tid_returns_error(void);
extern void test_thread_priority_set_invalid_tid_returns_error(void);
extern void test_thread_create_valid_attr_returns_valid_tid(void);
extern void test_thread_create_priority_zero(void);
extern void test_thread_create_priority_max(void);
extern void test_thread_create_user_privilege(void);
extern void test_thread_create_driver_privilege(void);
extern void test_thread_create_two_threads_have_distinct_tids(void);
extern void test_thread_create_name_exactly_15_chars(void);
extern void test_thread_create_name_longer_than_15_truncated(void);
extern void test_thread_create_minimal_stack_rounds_up_to_alignment(void);
extern void test_thread_create_null_attr_returns_error(void);
extern void test_thread_create_null_entry_returns_error(void);
extern void test_thread_create_zero_stack_returns_error(void);
extern void test_thread_create_overflow_resource_table_returns_enospc(void);
extern void test_thread_kill_valid_tid_returns_zero(void);
extern void test_thread_kill_invalid_tid_returns_error(void);
extern void test_thread_kill_self_returns_error(void);
extern void test_thread_kill_already_dead_returns_error(void);
extern void test_thread_suspend_and_resume_returns_zero(void);
extern void test_thread_suspend_twice_requires_two_resumes(void);
extern void test_thread_resume_non_suspended_is_noop(void);
extern void test_thread_suspend_invalid_tid_returns_error(void);
extern void test_thread_resume_invalid_tid_returns_error(void);
*/

/* =========================================================================
 * test_ipc_api.c
 * ========================================================================= */
/*
extern void test_ep_create_returns_valid_handle(void);
extern void test_ep_create_multiple_returns_distinct_handles(void);
extern void test_ep_create_three_handles_all_distinct(void);
extern void test_ep_create_up_to_system_limit_returns_enospc(void);
extern void test_msg_label_zero_is_valid(void);
extern void test_msg_label_max_is_valid(void);
extern void test_msg_words_are_independently_addressable(void);
extern void test_msg_copy_is_independent(void);
extern void test_ep_call_invalid_ep_returns_error(void);
extern void test_ep_call_null_msg_returns_error(void);
extern void test_ep_call_own_ep_returns_edeadlk(void);
extern void test_ep_recv_invalid_ep_returns_error(void);
extern void test_ep_recv_null_msg_returns_error(void);
extern void test_ep_recv_null_sender_returns_error(void);
extern void test_ep_reply_invalid_sender_returns_error(void);
extern void test_ep_reply_non_blocked_sender_returns_error(void);
extern void test_ep_reply_null_reply_returns_error(void);
extern void test_ep_reply_recv_invalid_ep_returns_error(void);
extern void test_ep_reply_recv_null_args_return_error(void);
extern void test_ep_grant_to_valid_target_returns_zero(void);
extern void test_ep_grant_invalid_ep_returns_error(void);
extern void test_ep_grant_invalid_target_returns_error(void);
extern void test_ep_grant_both_invalid_returns_error(void);
*/

/* =========================================================================
 * test_notif_api.c
 * ========================================================================= */
/*
extern void test_notif_create_returns_valid_handle(void);
extern void test_notif_create_multiple_returns_distinct_handles(void);
extern void test_notif_create_fresh_object_has_no_bits(void);
extern void test_notif_create_up_to_system_limit_returns_error(void);
extern void test_notif_signal_single_bit_visible_via_poll(void);
extern void test_notif_signal_multiple_bits_visible_via_poll(void);
extern void test_notif_signal_all_bits(void);
extern void test_notif_signal_bit_0(void);
extern void test_notif_signal_bit_31(void);
extern void test_notif_signal_zero_is_noop(void);
extern void test_notif_signal_accumulates_across_calls(void);
extern void test_notif_signal_idempotent(void);
extern void test_notif_signal_invalid_handle_does_not_crash(void);
extern void test_notif_poll_returns_matched_bits_only(void);
extern void test_notif_poll_clears_matched_bits(void);
extern void test_notif_poll_does_not_clear_unmatched_bits(void);
extern void test_notif_poll_after_poll_returns_zero(void);
extern void test_notif_poll_zero_mask_returns_zero(void);
extern void test_notif_poll_unmatched_mask_returns_zero(void);
extern void test_notif_poll_unmatched_mask_does_not_clear_bits(void);
extern void test_notif_poll_invalid_handle_returns_zero(void);
extern void test_ep_recv_or_notif_invalid_ep_returns_error(void);
extern void test_ep_recv_or_notif_invalid_notif_returns_error(void);
extern void test_ep_recv_or_notif_null_out_args_returns_error(void);
*/

/* =========================================================================
 * test_mem_api.c
 * ========================================================================= */
/*
extern void test_mem_map_anon_returns_non_null(void);
extern void test_mem_map_anon_result_is_8byte_aligned(void);
extern void test_mem_map_anon_region_is_readable(void);
extern void test_mem_map_anon_region_is_writable(void);
extern void test_mem_map_anon_region_is_zeroed(void);
extern void test_mem_map_anon_read_only_perms(void);
extern void test_mem_map_anon_two_regions_do_not_overlap(void);
extern void test_mem_map_size_1_rounds_up_to_alignment(void);
extern void test_mem_map_size_8_exact_alignment(void);
extern void test_mem_map_wx_write_exec_strips_exec(void);
extern void test_mem_map_exec_only_no_write_allowed(void);
extern void test_mem_map_size_zero_returns_null(void);
extern void test_mem_map_no_perms_returns_null(void);
extern void test_mem_map_exhausts_pool_returns_null(void);
extern void test_mem_map_periph_without_cap_returns_eperm(void);
extern void test_mem_map_periph_unknown_base_returns_null(void);
extern void test_mem_map_periph_null_base_returns_null(void);
extern void test_mem_unmap_valid_ptr_returns_zero(void);
extern void test_mem_map_unmap_cycle_repeated(void);
extern void test_mem_unmap_null_addr_returns_error(void);
extern void test_mem_unmap_unknown_addr_returns_error(void);
extern void test_mem_unmap_double_free_returns_error(void);
extern void test_mem_unmap_size_zero_returns_error(void);
extern void test_mem_grant_read_only_from_rw_source_returns_zero(void);
extern void test_mem_grant_same_perms_as_source_returns_zero(void);
extern void test_mem_grant_perms_clamped_to_source(void);
extern void test_mem_grant_null_addr_returns_error(void);
extern void test_mem_grant_unknown_addr_returns_error(void);
extern void test_mem_grant_invalid_target_returns_error(void);
extern void test_mem_grant_zero_size_returns_error(void);
extern void test_mem_grant_unmapped_region_returns_error(void);
*/

/* =========================================================================
 * test_irq_api.c
 * ========================================================================= */
/*
extern void test_irq_bind_valid_srpn_and_notif_returns_zero(void);
extern void test_irq_bind_bit_0(void);
extern void test_irq_bind_bit_31(void);
extern void test_irq_bind_rebind_same_srpn_to_different_notif_returns_zero(void);
extern void test_irq_bind_max_valid_srpn(void);
extern void test_irq_bind_multiple_srpns_distinct_notifs(void);
extern void test_irq_bind_invalid_notif_returns_error(void);
extern void test_irq_bind_reserved_srpn_zero_returns_error(void);
extern void test_irq_bind_bit_zero_mask_returns_error(void);
extern void test_irq_enable_after_bind_returns_zero(void);
extern void test_irq_disable_after_enable_returns_zero(void);
extern void test_irq_enable_disable_repeated_does_not_crash(void);
extern void test_irq_disable_already_disabled_is_noop(void);
extern void test_irq_enable_unbound_srpn_returns_error(void);
extern void test_irq_disable_unbound_srpn_returns_error(void);
extern void test_irq_enable_reserved_srpn_returns_error(void);
extern void test_irq_ack_enabled_srpn_returns_zero(void);
extern void test_irq_ack_unbound_srpn_returns_error(void);
extern void test_irq_ack_reserved_srpn_returns_error(void);
*/

/* =========================================================================
 * test_libul.c
 * ========================================================================= */
/*
extern void test_mutex_init_returns_zero(void);
extern void test_mutex_init_token_bit_is_available(void);
extern void test_mutex_lock_then_unlock_cycle(void);
extern void test_mutex_unlock_restores_token_bit(void);
extern void test_mutex_init_null_returns_error(void);
extern void test_sem_init_count_1_returns_zero(void);
extern void test_sem_init_count_8_returns_zero(void);
extern void test_sem_init_count_32_returns_zero(void);
extern void test_sem_post_then_poll_shows_extra_bit(void);
extern void test_sem_init_count_0_creates_fully_blocked_sem(void);
extern void test_sem_init_count_33_returns_error(void);
extern void test_sem_init_null_returns_error(void);
extern void test_queue_init_returns_zero(void);
extern void test_queue_capacity_equals_floor_of_buf_minus_header_over_item(void);
extern void test_queue_send_nonblock_then_recv_nonblock_roundtrip(void);
extern void test_queue_preserves_fifo_order(void);
extern void test_queue_fill_then_drain(void);
extern void test_queue_send_full_nonblock_returns_error(void);
extern void test_queue_recv_empty_nonblock_returns_error(void);
extern void test_queue_init_null_buf_returns_error(void);
extern void test_queue_init_item_larger_than_buf_returns_error(void);
extern void test_event_create_returns_valid_handle(void);
extern void test_event_set_bits_visible_via_wait_any(void);
extern void test_event_wait_all_requires_all_bits_to_be_set(void);
extern void test_event_clear_removes_specific_bits(void);
extern void test_event_set_zero_is_noop(void);
extern void test_event_clear_more_bits_than_set_does_not_crash(void);
extern void test_timer_oneshot_invalid_ep_returns_error(void);
extern void test_timer_periodic_invalid_ep_returns_error(void);
extern void test_timer_cancel_invalid_id_returns_error(void);
extern void test_timer_oneshot_zero_ticks_returns_error(void);
extern void test_timer_oneshot_zero_bit_mask_returns_error(void);
extern void test_pipe_init_returns_zero(void);
extern void test_pipe_write_then_read_single_byte(void);
extern void test_pipe_write_multiple_bytes_preserves_order(void);
extern void test_pipe_read_empty_nonblock_returns_error(void);
extern void test_pipe_write_full_nonblock_returns_error(void);
*/

int main(void)
{
	UNITY_BEGIN();

	/* --- Always active ------------------------------------------------- */
	RUN_TEST(test_dummy_one_plus_one);
	RUN_TEST(test_dummy_qemu_output);

	/* --- Thread API (uncomment when kernel is implemented) -------------- */
	/*
	RUN_TEST(test_thread_self_returns_valid_tid);
	RUN_TEST(test_thread_self_is_stable);
	RUN_TEST(test_thread_self_is_non_negative);
	RUN_TEST(test_thread_yield_returns);
	RUN_TEST(test_thread_yield_multiple_times_safe);
	RUN_TEST(test_thread_priority_get_self_in_range);
	RUN_TEST(test_thread_priority_set_and_get_roundtrip);
	RUN_TEST(test_thread_priority_set_min);
	RUN_TEST(test_thread_priority_set_max);
	RUN_TEST(test_thread_priority_set_same_value_twice);
	RUN_TEST(test_thread_priority_get_invalid_tid_returns_error);
	RUN_TEST(test_thread_priority_set_invalid_tid_returns_error);
	RUN_TEST(test_thread_create_valid_attr_returns_valid_tid);
	RUN_TEST(test_thread_create_priority_zero);
	RUN_TEST(test_thread_create_priority_max);
	RUN_TEST(test_thread_create_user_privilege);
	RUN_TEST(test_thread_create_driver_privilege);
	RUN_TEST(test_thread_create_two_threads_have_distinct_tids);
	RUN_TEST(test_thread_create_name_exactly_15_chars);
	RUN_TEST(test_thread_create_name_longer_than_15_truncated);
	RUN_TEST(test_thread_create_minimal_stack_rounds_up_to_alignment);
	RUN_TEST(test_thread_create_null_attr_returns_error);
	RUN_TEST(test_thread_create_null_entry_returns_error);
	RUN_TEST(test_thread_create_zero_stack_returns_error);
	RUN_TEST(test_thread_create_overflow_resource_table_returns_enospc);
	RUN_TEST(test_thread_kill_valid_tid_returns_zero);
	RUN_TEST(test_thread_kill_invalid_tid_returns_error);
	RUN_TEST(test_thread_kill_self_returns_error);
	RUN_TEST(test_thread_kill_already_dead_returns_error);
	RUN_TEST(test_thread_suspend_and_resume_returns_zero);
	RUN_TEST(test_thread_suspend_twice_requires_two_resumes);
	RUN_TEST(test_thread_resume_non_suspended_is_noop);
	RUN_TEST(test_thread_suspend_invalid_tid_returns_error);
	RUN_TEST(test_thread_resume_invalid_tid_returns_error);
	*/

	/* --- IPC Endpoint API (uncomment when kernel is implemented) -------- */
	/*
	RUN_TEST(test_ep_create_returns_valid_handle);
	RUN_TEST(test_ep_create_multiple_returns_distinct_handles);
	RUN_TEST(test_ep_create_three_handles_all_distinct);
	RUN_TEST(test_ep_create_up_to_system_limit_returns_enospc);
	RUN_TEST(test_msg_label_zero_is_valid);
	RUN_TEST(test_msg_label_max_is_valid);
	RUN_TEST(test_msg_words_are_independently_addressable);
	RUN_TEST(test_msg_copy_is_independent);
	RUN_TEST(test_ep_call_invalid_ep_returns_error);
	RUN_TEST(test_ep_call_null_msg_returns_error);
	RUN_TEST(test_ep_call_own_ep_returns_edeadlk);
	RUN_TEST(test_ep_recv_invalid_ep_returns_error);
	RUN_TEST(test_ep_recv_null_msg_returns_error);
	RUN_TEST(test_ep_recv_null_sender_returns_error);
	RUN_TEST(test_ep_reply_invalid_sender_returns_error);
	RUN_TEST(test_ep_reply_non_blocked_sender_returns_error);
	RUN_TEST(test_ep_reply_null_reply_returns_error);
	RUN_TEST(test_ep_reply_recv_invalid_ep_returns_error);
	RUN_TEST(test_ep_reply_recv_null_args_return_error);
	RUN_TEST(test_ep_grant_to_valid_target_returns_zero);
	RUN_TEST(test_ep_grant_invalid_ep_returns_error);
	RUN_TEST(test_ep_grant_invalid_target_returns_error);
	RUN_TEST(test_ep_grant_both_invalid_returns_error);
	*/

	/* --- Notification API (uncomment when kernel is implemented) -------- */
	/*
	RUN_TEST(test_notif_create_returns_valid_handle);
	RUN_TEST(test_notif_create_multiple_returns_distinct_handles);
	RUN_TEST(test_notif_create_fresh_object_has_no_bits);
	RUN_TEST(test_notif_create_up_to_system_limit_returns_error);
	RUN_TEST(test_notif_signal_single_bit_visible_via_poll);
	RUN_TEST(test_notif_signal_multiple_bits_visible_via_poll);
	RUN_TEST(test_notif_signal_all_bits);
	RUN_TEST(test_notif_signal_bit_0);
	RUN_TEST(test_notif_signal_bit_31);
	RUN_TEST(test_notif_signal_zero_is_noop);
	RUN_TEST(test_notif_signal_accumulates_across_calls);
	RUN_TEST(test_notif_signal_idempotent);
	RUN_TEST(test_notif_signal_invalid_handle_does_not_crash);
	RUN_TEST(test_notif_poll_returns_matched_bits_only);
	RUN_TEST(test_notif_poll_clears_matched_bits);
	RUN_TEST(test_notif_poll_does_not_clear_unmatched_bits);
	RUN_TEST(test_notif_poll_after_poll_returns_zero);
	RUN_TEST(test_notif_poll_zero_mask_returns_zero);
	RUN_TEST(test_notif_poll_unmatched_mask_returns_zero);
	RUN_TEST(test_notif_poll_unmatched_mask_does_not_clear_bits);
	RUN_TEST(test_notif_poll_invalid_handle_returns_zero);
	RUN_TEST(test_ep_recv_or_notif_invalid_ep_returns_error);
	RUN_TEST(test_ep_recv_or_notif_invalid_notif_returns_error);
	RUN_TEST(test_ep_recv_or_notif_null_out_args_returns_error);
	*/

	/* --- Memory API (uncomment when kernel is implemented) -------------- */
	/*
	RUN_TEST(test_mem_map_anon_returns_non_null);
	RUN_TEST(test_mem_map_anon_result_is_8byte_aligned);
	RUN_TEST(test_mem_map_anon_region_is_readable);
	RUN_TEST(test_mem_map_anon_region_is_writable);
	RUN_TEST(test_mem_map_anon_region_is_zeroed);
	RUN_TEST(test_mem_map_anon_read_only_perms);
	RUN_TEST(test_mem_map_anon_two_regions_do_not_overlap);
	RUN_TEST(test_mem_map_size_1_rounds_up_to_alignment);
	RUN_TEST(test_mem_map_size_8_exact_alignment);
	RUN_TEST(test_mem_map_wx_write_exec_strips_exec);
	RUN_TEST(test_mem_map_exec_only_no_write_allowed);
	RUN_TEST(test_mem_map_size_zero_returns_null);
	RUN_TEST(test_mem_map_no_perms_returns_null);
	RUN_TEST(test_mem_map_exhausts_pool_returns_null);
	RUN_TEST(test_mem_map_periph_without_cap_returns_eperm);
	RUN_TEST(test_mem_map_periph_unknown_base_returns_null);
	RUN_TEST(test_mem_map_periph_null_base_returns_null);
	RUN_TEST(test_mem_unmap_valid_ptr_returns_zero);
	RUN_TEST(test_mem_map_unmap_cycle_repeated);
	RUN_TEST(test_mem_unmap_null_addr_returns_error);
	RUN_TEST(test_mem_unmap_unknown_addr_returns_error);
	RUN_TEST(test_mem_unmap_double_free_returns_error);
	RUN_TEST(test_mem_unmap_size_zero_returns_error);
	RUN_TEST(test_mem_grant_read_only_from_rw_source_returns_zero);
	RUN_TEST(test_mem_grant_same_perms_as_source_returns_zero);
	RUN_TEST(test_mem_grant_perms_clamped_to_source);
	RUN_TEST(test_mem_grant_null_addr_returns_error);
	RUN_TEST(test_mem_grant_unknown_addr_returns_error);
	RUN_TEST(test_mem_grant_invalid_target_returns_error);
	RUN_TEST(test_mem_grant_zero_size_returns_error);
	RUN_TEST(test_mem_grant_unmapped_region_returns_error);
	*/

	/* --- IRQ API (uncomment when kernel is implemented) ----------------- */
	/*
	RUN_TEST(test_irq_bind_valid_srpn_and_notif_returns_zero);
	RUN_TEST(test_irq_bind_bit_0);
	RUN_TEST(test_irq_bind_bit_31);
	RUN_TEST(test_irq_bind_rebind_same_srpn_to_different_notif_returns_zero);
	RUN_TEST(test_irq_bind_max_valid_srpn);
	RUN_TEST(test_irq_bind_multiple_srpns_distinct_notifs);
	RUN_TEST(test_irq_bind_invalid_notif_returns_error);
	RUN_TEST(test_irq_bind_reserved_srpn_zero_returns_error);
	RUN_TEST(test_irq_bind_bit_zero_mask_returns_error);
	RUN_TEST(test_irq_enable_after_bind_returns_zero);
	RUN_TEST(test_irq_disable_after_enable_returns_zero);
	RUN_TEST(test_irq_enable_disable_repeated_does_not_crash);
	RUN_TEST(test_irq_disable_already_disabled_is_noop);
	RUN_TEST(test_irq_enable_unbound_srpn_returns_error);
	RUN_TEST(test_irq_disable_unbound_srpn_returns_error);
	RUN_TEST(test_irq_enable_reserved_srpn_returns_error);
	RUN_TEST(test_irq_ack_enabled_srpn_returns_zero);
	RUN_TEST(test_irq_ack_unbound_srpn_returns_error);
	RUN_TEST(test_irq_ack_reserved_srpn_returns_error);
	*/

	/* --- libul (uncomment when kernel is implemented) ------------------- */
	/*
	RUN_TEST(test_mutex_init_returns_zero);
	RUN_TEST(test_mutex_init_token_bit_is_available);
	RUN_TEST(test_mutex_lock_then_unlock_cycle);
	RUN_TEST(test_mutex_unlock_restores_token_bit);
	RUN_TEST(test_mutex_init_null_returns_error);
	RUN_TEST(test_sem_init_count_1_returns_zero);
	RUN_TEST(test_sem_init_count_8_returns_zero);
	RUN_TEST(test_sem_init_count_32_returns_zero);
	RUN_TEST(test_sem_post_then_poll_shows_extra_bit);
	RUN_TEST(test_sem_init_count_0_creates_fully_blocked_sem);
	RUN_TEST(test_sem_init_count_33_returns_error);
	RUN_TEST(test_sem_init_null_returns_error);
	RUN_TEST(test_queue_init_returns_zero);
	RUN_TEST(test_queue_capacity_equals_floor_of_buf_minus_header_over_item);
	RUN_TEST(test_queue_send_nonblock_then_recv_nonblock_roundtrip);
	RUN_TEST(test_queue_preserves_fifo_order);
	RUN_TEST(test_queue_fill_then_drain);
	RUN_TEST(test_queue_send_full_nonblock_returns_error);
	RUN_TEST(test_queue_recv_empty_nonblock_returns_error);
	RUN_TEST(test_queue_init_null_buf_returns_error);
	RUN_TEST(test_queue_init_item_larger_than_buf_returns_error);
	RUN_TEST(test_event_create_returns_valid_handle);
	RUN_TEST(test_event_set_bits_visible_via_wait_any);
	RUN_TEST(test_event_wait_all_requires_all_bits_to_be_set);
	RUN_TEST(test_event_clear_removes_specific_bits);
	RUN_TEST(test_event_set_zero_is_noop);
	RUN_TEST(test_event_clear_more_bits_than_set_does_not_crash);
	RUN_TEST(test_timer_oneshot_invalid_ep_returns_error);
	RUN_TEST(test_timer_periodic_invalid_ep_returns_error);
	RUN_TEST(test_timer_cancel_invalid_id_returns_error);
	RUN_TEST(test_timer_oneshot_zero_ticks_returns_error);
	RUN_TEST(test_timer_oneshot_zero_bit_mask_returns_error);
	RUN_TEST(test_pipe_init_returns_zero);
	RUN_TEST(test_pipe_write_then_read_single_byte);
	RUN_TEST(test_pipe_write_multiple_bytes_preserves_order);
	RUN_TEST(test_pipe_read_empty_nonblock_returns_error);
	RUN_TEST(test_pipe_write_full_nonblock_returns_error);
	*/

	return UNITY_END();
}

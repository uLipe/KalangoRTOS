#include <sched.h>

KernelResult SchedulerDoRoundRobin(TaskPriorityList *list) {
    ASSERT_PARAM(list);

    uint8_t top_priority = (31 - ArchCountLeadZeros(list->ready_task_bitmap));
    
    IrqDisable();
    sys_dnode_t *current_head = sys_dlist_peek_head(&list->task_list[top_priority]);
    sys_dnode_t *next = sys_dlist_peek_next(&list->task_list[top_priority], current_head);

    //The list has at least one more element, round robin allowed:
    if(next) {
        //move current head to the tail of the list, dont worry to 
        //scheduling, the core module is responsible to manage
        //ready lists
        sys_dlist_remove(current_head);
        sys_dlist_append(&list->task_list[top_priority], current_head);
    }

    IrqEnable();
    return kSuccess;
}
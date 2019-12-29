#include <sched.h>


bool IsSchedulerLocked(TaskPriorityList *taskset) {

    IrqDisable();
    bool result = ((taskset->lock_level == 0) ? false : true); 
    IrqEnable();

    return result;
}

KernelResult SchedulerLock(TaskPriorityList *taskset) {
    ASSERT_PARAM(taskset);

    IrqDisable();
    if(taskset->lock_level < 0xFFFFFFFF)
        taskset->lock_level++;
    IrqEnable();

    return kStatusSchedLocked;
}

KernelResult SchedulerUnlock(TaskPriorityList *taskset) {
    ASSERT_PARAM(taskset);

    IrqDisable();
    if(taskset->lock_level > 0x0)
        taskset->lock_level--;
    IrqEnable();

    return (taskset->lock_level) ? kStatusSchedLocked : kStatusSchedUnlocked;
}


TaskControBlock *ScheduleTaskSet(TaskPriorityList *taskset) {
    ASSERT_KERNEL(taskset, NULL);   

    if(taskset->lock_level) {
        return NULL;
    } 

    IrqDisable();

    uint8_t top_priority = (31 - ArchCountLeadZeros(taskset->ready_task_bitmap));
    sys_dnode_t *node = NULL;
    TaskControBlock *top_priority_task = NULL;

    node = sys_dlist_peek_head(&taskset->task_list[top_priority]);
    top_priority_task = CONTAINER_OF(node, TaskControBlock, ready_node);

    IrqEnable();

    return (top_priority_task);
}

void SchedulerInitTaskPriorityList(TaskPriorityList *list) {
    IrqDisable();
    
    for(uint32_t i = 0; i < CONFIG_PRIORITY_LEVELS; i++) {
        sys_dlist_init(&list->task_list[i]);
    }
    list->ready_task_bitmap = 0;
    list->lock_level = 0;

    IrqEnable();
}

bool NothingToSched(TaskPriorityList *list) {
    ASSERT_PARAM(list);

    IrqDisable();
    bool result =  (list->ready_task_bitmap == 0 ? true : false );
    IrqEnable();

    return result;
}

KernelResult SchedulerSetPriority(TaskPriorityList *list, uint32_t priority) {
    ASSERT_PARAM(list);
    ASSERT_PARAM(priority < CONFIG_PRIORITY_LEVELS);

    IrqDisable();
    list->ready_task_bitmap |= (1 << priority);
    IrqEnable();
    return kSuccess;
}

KernelResult SchedulerResetPriority(TaskPriorityList *list, uint32_t priority) {
    ASSERT_PARAM(list);
    ASSERT_PARAM(priority < CONFIG_PRIORITY_LEVELS);

    IrqDisable();
    if(sys_dlist_is_empty(&list->task_list[priority])) {
        list->ready_task_bitmap &= ~(1 << priority);
        IrqEnable();
        return kSuccess;
    } else {
        IrqEnable();
        return kErrorBufferFull;
    }
}
#include <object_pool.h>

#if CONFIG_NOOF_TASKS < 2 
    #error "The kernel needs at least 1 user plus 1 kernel tcbs, check config file"
#endif    

static sys_dlist_t task_free_list = SYS_DLIST_STATIC_INIT(&task_free_list);

#if CONFIG_NOOF_SEMAPHORES > 0
static sys_dlist_t semaphore_free_list = SYS_DLIST_STATIC_INIT(&semaphore_free_list);
#endif

#if CONFIG_NOOF_MUTEXES > 0 
static sys_dlist_t mutex_free_list = SYS_DLIST_STATIC_INIT(&mutex_free_list);
#endif

#if CONFIG_NOOF_QUEUES > 0
static sys_dlist_t queue_free_list = SYS_DLIST_STATIC_INIT(&queue_free_list);
#endif

#if CONFIG_NOOF_TIMERS > 0
static sys_dlist_t timer_free_list = SYS_DLIST_STATIC_INIT(&timer_free_list);
#endif
static TaskControBlock tasks_pool[CONFIG_NOOF_TASKS + 1]; //extra object for idle task

#if CONFIG_NOOF_SEMAPHORES > 0
static Semaphore semaphore_pool[CONFIG_NOOF_SEMAPHORES];
#endif

#if CONFIG_NOOF_MUTEXES > 0 
static Mutex mutex_pool[CONFIG_NOOF_MUTEXES];
#endif

#if CONFIG_NOOF_QUEUES > 0
static Queue queue_pool[CONFIG_NOOF_QUEUES];
#endif

#if CONFIG_NOOF_TIMERS > 0
static Timer timer_pool[CONFIG_NOOF_TIMERS];
#endif

static bool pools_initialized = false;

KernelResult InitializeObjectPools() {
    if(pools_initialized)
        return kSuccess;

    pools_initialized = true;

    for(uint32_t i = 0; i < CONFIG_NOOF_TASKS; i++) {
        sys_dlist_append(&task_free_list, &tasks_pool[i].pool_node);
    }

#if CONFIG_NOOF_SEMAPHORES > 0
    for(uint32_t i = 0; i < CONFIG_NOOF_SEMAPHORES; i++) {
        sys_dlist_append(&semaphore_free_list, &semaphore_pool[i].pool_node);
    }
#endif

#if CONFIG_NOOF_MUTEXES > 0 
    for(uint32_t i = 0; i < CONFIG_NOOF_MUTEXES; i++) {
        sys_dlist_append(&mutex_free_list, &mutex_pool[i].pool_node);
    }
#endif

#if CONFIG_NOOF_QUEUES > 0
    for(uint32_t i = 0; i < CONFIG_NOOF_QUEUES; i++) {
        sys_dlist_append(&queue_free_list, &queue_pool[i].pool_node);
    }
#endif

#if CONFIG_NOOF_TIMERS > 0
    for(uint32_t i = 0; i < CONFIG_NOOF_TIMERS; i++) {
        sys_dlist_append(&timer_free_list, &timer_pool[i].pool_node);
    }
#endif

    return kSuccess;
}

TaskControBlock *AllocateTaskObject() {
    TaskControBlock *task = NULL;

    if(sys_dlist_is_empty(&task_free_list)){
        return task;
    }

    sys_dnode_t *head = sys_dlist_peek_head(&task_free_list);
    sys_dlist_remove(head);

    task = CONTAINER_OF(head, TaskControBlock, pool_node);
    return (task);
}

KernelResult FreeTaskObject(TaskControBlock *self) {
    sys_dlist_append(&task_free_list, &self->pool_node);
    return kSuccess;
}

#if CONFIG_NOOF_SEMAPHORES > 0
Semaphore *AllocateSemaphoreObject() {

    Semaphore *semaphore = NULL;

    if(sys_dlist_is_empty(&semaphore_free_list)){
        return semaphore;
    }

    sys_dnode_t *head = sys_dlist_peek_head(&semaphore_free_list);
    sys_dlist_remove(head);

    semaphore = CONTAINER_OF(head, Semaphore, pool_node);
    return (semaphore);
}

KernelResult FreeSemaphoreObject(Semaphore *self) {
    sys_dlist_append(&semaphore_free_list, &self->pool_node);
    return kSuccess;
}
#endif

#if CONFIG_NOOF_MUTEXES > 0 
Mutex *AllocateMutexObject() {

    Mutex *mutex = NULL;

    if(sys_dlist_is_empty(&mutex_free_list)){
        return mutex;
    }

    sys_dnode_t *head = sys_dlist_peek_head(&mutex_free_list);
    sys_dlist_remove(head);

    mutex = CONTAINER_OF(head, Mutex, pool_node);
    return (mutex);

}

KernelResult FreeMutexObject(Mutex *self) {
    sys_dlist_append(&mutex_free_list, &self->pool_node);
    return kSuccess;
}
#endif

#if CONFIG_NOOF_TIMERS > 0
Timer *AllocateTimerObject() {

    Timer *timer = NULL;

    if(sys_dlist_is_empty(&timer_free_list)){
        return timer;
    }

    sys_dnode_t *head = sys_dlist_peek_head(&timer_free_list);
    sys_dlist_remove(head);

    timer = CONTAINER_OF(head, Timer, pool_node);
    return (timer);

}

KernelResult FreeTimerObject(Timer *self) {
    sys_dlist_append(&timer_free_list, &self->pool_node);
    return kSuccess;
}
#endif

#if CONFIG_NOOF_QUEUES > 0
Queue *AllocateQueueObject() {

    Queue *queue = NULL;

    if(sys_dlist_is_empty(&queue_free_list)){
        return queue;
    }

    sys_dnode_t *head = sys_dlist_peek_head(&queue_free_list);
    sys_dlist_remove(head);

    queue = CONTAINER_OF(head, Queue, pool_node);
    return (queue);

}

KernelResult FreeQueueObject(Queue *self) {
    sys_dlist_append(&queue_free_list, &self->pool_node);
    return kSuccess;
}
#endif
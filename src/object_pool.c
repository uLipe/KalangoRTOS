#include <object_pool.h>

#if CONFIG_KERNEL_BLOCKS < 1
    #error "Kernel needs at least a single memory block"
#endif

typedef struct {
    uint8_t block[sizeof(Queue)];
    sys_dnode_t pool_node;
}KernelBlock;

static sys_dlist_t kernel_blocks_free = SYS_DLIST_STATIC_INIT(&kernel_blocks_free);
static KernelBlock kernel_blocks[CONFIG_KERNEL_BLOCKS + 1];
static bool pools_initialized = false;

KernelResult InitializeObjectPools() {
    if(pools_initialized)
        return kSuccess;

    pools_initialized = true;

    for(uint32_t i = 0; i < CONFIG_KERNEL_BLOCKS + 1; i++) {
        sys_dlist_append(&kernel_blocks_free, &kernel_blocks[i].pool_node);
    }

    return kSuccess;
}

static sys_dnode_t *AllocateKernelBlock() {
    if(sys_dlist_is_empty(&kernel_blocks_free)){
        return NULL;
    }

    sys_dnode_t *head = sys_dlist_peek_head(&kernel_blocks_free);
    sys_dlist_remove(head);

    return(head);
}

static void FreeKernelBlock(sys_dnode_t *pool_node) {
    if(pool_node) {
        sys_dlist_append(&kernel_blocks_free, pool_node);
    }
}

TaskControBlock *AllocateTaskObject() {
    TaskControBlock *task = NULL;
    task = (TaskControBlock *)CONTAINER_OF(AllocateKernelBlock(), KernelBlock, pool_node);
    return (task);
}

KernelResult FreeTaskObject(TaskControBlock *self) {
    KernelBlock *block = (KernelBlock *)self; 
    FreeKernelBlock(&block->pool_node);
    return kSuccess;
}

#if CONFIG_NOOF_SEMAPHORES > 0
Semaphore *AllocateSemaphoreObject() {

    Semaphore *semaphore = NULL;
    semaphore = (Semaphore *)CONTAINER_OF(AllocateKernelBlock(), KernelBlock, pool_node);
    return (semaphore);
}

KernelResult FreeSemaphoreObject(Semaphore *self) {
    KernelBlock *block = (KernelBlock *)self; 
    FreeKernelBlock(&block->pool_node);
    return kSuccess;
}
#endif

#if CONFIG_NOOF_MUTEXES > 0 
Mutex *AllocateMutexObject() {

    Mutex *mutex = NULL;
    mutex = (Mutex *)CONTAINER_OF(AllocateKernelBlock(), KernelBlock, pool_node);
    return (mutex);
}

KernelResult FreeMutexObject(Mutex *self) {
    KernelBlock *block = (KernelBlock *)self; 
    FreeKernelBlock(&block->pool_node);
    return kSuccess;
}
#endif

#if CONFIG_NOOF_TIMERS > 0
Timer *AllocateTimerObject() {

    Timer *timer = NULL;
    timer = (Timer *)CONTAINER_OF(AllocateKernelBlock(), KernelBlock, pool_node);
    return (timer);

}

KernelResult FreeTimerObject(Timer *self) {
    KernelBlock *block = (KernelBlock *)self; 
    FreeKernelBlock(&block->pool_node);
    return kSuccess;
}
#endif

#if CONFIG_NOOF_QUEUES > 0
Queue *AllocateQueueObject() {

    Queue *queue = NULL;
    queue = (Queue *)CONTAINER_OF(AllocateKernelBlock(), KernelBlock, pool_node);
    return (queue);

}

KernelResult FreeQueueObject(Queue *self) {
    KernelBlock *block = (KernelBlock *)self; 
    FreeKernelBlock(&block->pool_node);
    return kSuccess;
}
#endif
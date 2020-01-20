#include <object_pool.h>
#include <tlsf.h>

#ifndef CONFIG_KERNEL_HEAP_SIZE
#warning "Heap size was not defined, defaulting to 4096 bytes"
#define CONFIG_KERNEL_HEAP_SIZE 4096
#endif

static uint8_t kernel_heap[(CONFIG_KERNEL_HEAP_SIZE + sizeof(control_t) + ALIGN_SIZE) & ~(ALIGN_SIZE - 1)];
static tlsf_t  kernel_tlsf = NULL;
static uint32_t kernel_heap_free_bytes = CONFIG_KERNEL_HEAP_SIZE;

KernelResult InitializeObjectPools() {
    IrqDisable();
    kernel_tlsf = tlsf_create_with_pool(&kernel_heap, sizeof(kernel_heap));
    IrqEnable();

    if(kernel_tlsf) {
        return kSuccess;
    } else  {
        return kErrorNotEnoughKernelMemory;
    }
}

static void *KMalloc(uint32_t size) {
    IrqDisable();
    void *result = tlsf_malloc(kernel_tlsf, size);
    IrqEnable();

    if(result) {
        kernel_heap_free_bytes -= size;
    }

    return result;
}

static void KFree(void *memory) {
    IrqDisable();

    if(memory) {
        kernel_heap_free_bytes += tlsf_block_size(memory);   
    }

    tlsf_free(kernel_tlsf, memory);
    IrqEnable();
}

uint32_t GetKernelFreeBytesOnHeap() {
    if(!kernel_tlsf) {
        return 0;
    } else {
        return kernel_heap_free_bytes;
    }
}

uint8_t *AllocateRawBuffer(uint32_t size) {
    return ((uint8_t *)KMalloc(size)); 
}

KernelResult FreeRawBuffer(uint8_t *self) {
    KFree(self);
    return kSuccess;
}


TaskControBlock *AllocateTaskObject() {
    TaskControBlock *task = KMalloc(sizeof(TaskControBlock));
    return (task);
}

KernelResult FreeTaskObject(TaskControBlock *self) {
    KFree(self);
    return kSuccess;
}

Semaphore *AllocateSemaphoreObject() {

    Semaphore *semaphore = KMalloc(sizeof(Semaphore));
    return (semaphore);
}

KernelResult FreeSemaphoreObject(Semaphore *self) {
    KFree(self);
    return kSuccess;
}

Mutex *AllocateMutexObject() {

    Mutex *mutex = KMalloc(sizeof(Mutex));
    return (mutex);
}

KernelResult FreeMutexObject(Mutex *self) {
    KFree(self);
    return kSuccess;
}

Timer *AllocateTimerObject() {

    Timer *timer = KMalloc(sizeof(Timer));
    return (timer);
}

KernelResult FreeTimerObject(Timer *self) {
    KFree(self);
    return kSuccess;
}

Queue *AllocateQueueObject() {
    Queue *queue = KMalloc(sizeof(Queue));
    return (queue);
}

KernelResult FreeQueueObject(Queue *self) {
    KFree(self);
    return kSuccess;
}

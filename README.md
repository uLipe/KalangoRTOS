# KalangoRTOS

KalangoRTOS is an experimental, “just for fun” real-time kernel designed to help learning and exploring multitasking internals on microcontrollers.
It is intentionally kept small and approachable, while still being scalable enough to evolve into more serious projects.

> Status: under development / experimental. APIs and internals may change.

---

## Main Features

- Real time preemptive scheduler;
- Fast and predictable execution time context switching;
- Supports up to 32 priority levels;
- Round-robin policy with same priority threads;
- Soft timers;
- Counting Semaphores;
- Binary Semaphores;
- Task management;
- Recursive mutexes;
- Message Queues;
- Scalable, user can configure how much kernel objects application need;
- Unlimited kernel objects and threads (limited by processor memory);
- O(1) TLSF memory allocator, leave kernel to manage its memory;
- Written in C with as few assembly paths as possible (mostly context switching);

---

## Limitations

- Please keep in mind this is an experimental project;
- Intended to support popular 32-bit microcontrollers (no plan to support 8-bit platforms);
- Most of the code is written with GCC or Clang in mind;
- No C++ support;
- Designed to take advantage of vendor abstraction libraries (e.g. CMSIS, NRFx);
- Timer callbacks are deferred from ISR;

---

## Get the Code

```bash
git clone https://github.com/uLipe/KalangoRTOS
````

---

## Getting Started (CMake Integration)

You can integrate KalangoRTOS using CMake by adding this repository as a subdirectory:

```cmake
add_subdirectory(KalangoRTOS)
```

Then link the library in your executable target:

```cmake
target_link_libraries(<your_executable_target> KalangoRTOS <other libraries>)
```

### Providing a configuration file

You must provide a `kalango_config.h`.
There are templates under `confs/`. Copy one to your project and rename it to `kalango_config.h`.

When running CMake, provide the config file location:

```bash
cmake <source-directory> <other-arguments> -DKALANGO_CONFIG_FILE_PATH=/path/to/kalango_config.h/file
```

---

## Getting Started (Stand-alone Mode)

* Add the `include/` folder to your include path;
* Add `src/`, the desired `archs/` implementation, and `utils/` folders to your build sources;
* Copy a config template from `confs/` into your project and rename it to `kalango_config.h`;
* Add to your compiler options:

```bash
-include<path/to/kalango_config.h/file>
```

* Include `kalango_api.h` in your application code to use RTOS features;
* In your `main()`, initialize your target and start scheduling by calling **`Kalango_CoreStart()`**.

Example:

```c
#include "kalango_api.h"

static TaskId task_a;
static TaskId task_b;

static void DemoTask1(void *arg) {
    uint32_t noof_wakeups = 0;
    (void)arg;

    for(;;) {
        Kalango_Sleep(250);
        noof_wakeups++;
    }
}

static void DemoTask2(void *arg) {
    uint32_t noof_wakeups = 0;
    (void)arg;

    for(;;) {
        Kalango_Sleep(25);
        noof_wakeups++;
    }
}

int main(void) {
    TaskSettings settings;

    settings.arg = NULL;
    settings.function = DemoTask1;
    settings.priority = 8;
    settings.stack_size = 512;

    task_a = Kalango_TaskCreate(&settings);

    settings.arg = NULL;
    settings.function = DemoTask2;
    settings.priority = 4;
    settings.stack_size = 512;

    task_b = Kalango_TaskCreate(&settings);

    (void)task_a;
    (void)task_b;

    // Start scheduling
    Kalango_CoreStart();
    return 0;
}
```

---

## Public API Usage (kalango_api.h)

This section summarizes practical usage patterns for the public API.

### Conventions

* **Timeouts**

  * `KERNEL_NO_WAIT` (0): return immediately if the resource is not available.
  * `KERNEL_WAIT_FOREVER` (-1): wait indefinitely.
* **ISR usage**

  * Some APIs can be called from an ISR only when wrapped by:

    * `Kalango_IrqEnter()` at ISR entry
    * `Kalango_IrqLeave()` at ISR exit

---

### Tasks

Create a task:

```c
static void WorkerTask(void *arg) {
    (void)arg;
    for(;;) {
        // do work
        Kalango_Sleep(10);
    }
}

void AppInit(void) {
    TaskSettings s = {
        .priority   = 2,
        .stack_size = 1024,
        .function   = WorkerTask,
        .arg        = NULL,
    };

    TaskId id = Kalango_TaskCreate(&s);
    if(!id) {
        // handle failure
    }
}
```

Suspend / Resume:

```c
Kalango_TaskSuspend(worker_id);
Kalango_TaskResume(worker_id);
```

Yield:

```c
KernelResult r = Kalango_TaskYield();
if(r != kSuccess) {
    // e.g. called from ISR
}
```

Priority (note: returns `uint32_t`, not `KernelResult`):

```c
uint32_t old = Kalango_TaskSetPriority(worker_id, 3);
if(old == 0xFFFFFFFF) {
    // invalid param / failure depending on implementation
}
```

---

### Semaphores

Binary semaphore usage:

```c
SemaphoreId sem = Kalango_SemaphoreCreate(0, 1);

// Task A: wait
KernelResult r = Kalango_SemaphoreTake(sem, KERNEL_WAIT_FOREVER);
if(r == kSuccess) {
    // acquired
}

// Task B: signal
Kalango_SemaphoreGive(sem, 1);
```

ISR “give” pattern (when supported by the port/kernel rules):

```c
void MyIRQ_Handler(void) {
    Kalango_IrqEnter();

    Kalango_SemaphoreGive(sem, 1);

    Kalango_IrqLeave();
}
```

---

### Mutex

```c
MutexId m = Kalango_MutexCreate();

KernelResult r = Kalango_MutexLock(m, KERNEL_WAIT_FOREVER);
if(r == kSuccess) {
    // protected section
    Kalango_MutexUnlock(m);
}
```

---

### Queues

```c
typedef struct {
    uint32_t a;
    uint32_t b;
} Msg;

QueueId q = Kalango_QueueCreate(8, sizeof(Msg));

// Producer
Msg out = { .a = 1, .b = 2 };
Kalango_QueueInsert(q, &out, sizeof(out), KERNEL_WAIT_FOREVER);

// Consumer
Msg in;
uint32_t sz = sizeof(in);
KernelResult r = Kalango_QueueRemove(q, &in, &sz, KERNEL_WAIT_FOREVER);
if(r == kSuccess && sz == sizeof(in)) {
    // got message
}
```

---

### Timers

Periodic timer:

```c
static void TimerCb(void *user) {
    (void)user;
    // timer callback
}

TimerId t = Kalango_TimerCreate(TimerCb, 100, 100, NULL); // every 100 ticks
Kalango_TimerStart(t);
```

Stop / delete:

```c
Kalango_TimerStop(t);
Kalango_TimerDelete(t);
```

---

### Critical Sections

```c
Kalango_CriticalEnter();
// critical region
Kalango_CriticalExit();
```

---

## Support

If you want help with this work, give a star and contact: [ryukokki.felipe@gmail.com](mailto:ryukokki.felipe@gmail.com)

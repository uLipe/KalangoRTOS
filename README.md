# Kalango, a always experimental just for fun RTOS:
Simple preemptive-cooperative, realtime, multitask kernel made just for fun and aims to be used
to learn basics and the internals of multitasking programming on microcontrollers, the kernel
is engineered to be simple and scalable allowing others to download, use, learn and scale it
into more professional projects. It is under development status and all updates will
figure here.

# Main Features:
- Real time preemptive scheduler;
- Fast and predictable execution time context switching;
- Supports up to 32 priority levels;
- round-robin policy with same priority threads;
- Soft timers;
- Counting Semaphores;
- Binary Semaphores;
- Task management;
- Recursive mutexes;
- Message Queues;
- Scalable, user can configure how much kernel objects application need;
- Unlimited kernel objects and threads(limited by processor memory);
- O(1) TLSF memory allocator,leave kernel to manage its memory;
- Written in C with less possible assembly paths (just on context switching);

# Limitations:
- Please keep in mind this is an experimental project;
- Intended to support popular 32bit microcontrollers, no plan to support 8-bit platforms;
- Most of the code are written with GCC or CLang in mind;
- No C++ support;
- It was designed to take advantage of exisiting manufacturers microcontroller abstraction libraries
such CMSIS and NRFx;
- Timer callbacks are deffered from ISR;

# Get the Code!
To get this respository:
 ```
 $ git clone https://github.com/uLipe/KalangoRTOS
 ```

# Getting started, using CMake:
You can use the CMake build system to integrate the Kalango into your existing cmake project, 
to do so just copy this folder into your project folder and in your cmake project search by this directory
as below:

```
    add_subdirectory(KalangoRTOS)
```

After that, in your executable target, just link Kalango library using regular CMake option:

```
    target_link_libraries(<your_executable_target> KalangoRTOS <other libraries>)
```

The Kalango top-level include will placed in your project and you can invoke all the Kalango related
functions.

Additionally you need to supply a kalango_config.h header file, inside of <b>confs</b> folder, there
are some samples, just copy to your project and rename it to <b>kalango_config.h</b>, after that
when running your project cmake command just supply the location of this file:

```
cmake <source-directory> <other-arguments> -DKALANGO_CONFIG_FILE_PATH=/path/to/kalango_config.h/file
```

# Getting started, stand-alone mode:
- On your embedded project, add <b>include</b> folder to your include search path;
- Add <b>src</b>, desired <b>arch</b> folder and <b>lib</b> folders to search source path;
- from confs board, take a template config and put in your project, rename it to kalango_config.h
- add to your compiling options:

 ```
 -include<path/to/kalango_config.h/file>
 ```

- include the kalango_api.h on your application code to use RTOS features;
- Inside of your main function, initialize your target then run the scheduler by calling
  <b>Kalango_CoreStart()</b> function. See example below.
 
 ```
#include "kalango_api.h"

static TaskId task_a; 
static TaskId task_b; 

static void DemoTask1(void *arg) {
    uint32_t noof_wakeups = 0;
    
    for(;;) {
        Kalango_Sleep(250);
        noof_wakeups++;
    }
}

static void DemoTask2(void *arg) {
    uint32_t noof_wakeups = 0;

    for(;;) {
        Kalango_Sleep(25);
        noof_wakeups++;
    }
}

int main (void) {
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

    //Start scheduling!
    Kalango_CoreStart();
    return 0;
}

```

# Support:
- If you want some help with this work give a star and contact me: ryukokki.felipe@gmail.com





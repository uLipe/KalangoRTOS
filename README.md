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
- Interrupt management (Register, Enable and Disable);
- Scalable, user can configure how much kernel objects application need;
- Unlimited kernel objects and threads(limited by processor memory);
- Written in C with less possible assembly paths (just on context switching);
- Samples included for popular boards;

# Limitations:
- Not fully (yet) tested, please keep in mind this is an experimental project;
- Intended to support popular 32bit microcontrollers, no plan to support 8-bit platforms;
- Most of the code are written with GCC or CLang in mind;
- No C++ support;
- It was designed to take advantage of exisiting manufacturers microcontroller abstraction libraries
such CMSIS and NRFx;
- No Round-Robin scheduler (yet);
- Documentation is in development (the code was written to be expressive as possible);
- No delta list for O(1) tick handling, timer callbacks are deffered from ISR;

# Getting started:
- The best way to get started is to using one of sample projects provided under examples folder:
 ```
 $ cd Kalango/examples/nrf52_examples
 ```
 - To play with the demos you will need the meson build system;
 - The meson.build file by default will build the blink demo, you can add your own code and modify it;
 - prepare the meson build:
 ```
 $ meson build --cross-file cross-file.txt
 ```
 - Then go to build folder and build the firmware:
 ```
 $ cd build
 $ ninja hex
 ```
 - both elf and hex files will be available inside build named with blink;
 - flash it into your chip, plug a debugger and enjoy.

# Support:
- If you want some help with this work give a star and contact me: ryukokki.felipe@gmail.com





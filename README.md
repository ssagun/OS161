# OS161

This is an implementation of an operating system for the Operating Systems course, CPEN 331, at the University of British Columbia.

## Overview
This is a project that involved implementing various portions of an operating system. The main goal was to gain a strengthened understanding of concurrency, synchronization, virtual memory, system calls, and file systems.

The course that this project was associated with describes the key aspects of the course as: Operating systems, their design and their implementation. Process concurrency, synchronization, communication and scheduling. Device drivers, memory management, virtual memory, file systems, networking and security.

## Implemented Features
### Concurrency
### Mutex Lock
A mutex lock is an struct that may be acquired only by one thread at a time. It operates using a waiting channel and a spinlock. When a thread wants to use the mutex lock, the implementation will acquire a spinlock (and spin on it if it not available), check the status of the lock flag, and sleep on the waiting channel / release the spinlock if the flag is not free.

### Condition Variable
A condition variable is a struct that threads may be placed on to sleep until some function calls upon it to wake a single thread, or all threads. Condition variables allow for synchronization as the act of going to sleep/waking up is an atomic action. They are often used for pausing until a condition is satisfied that allows for concurrent operations to occur safely.

### Process System Calls
System Call: getpid()
System Call: fork()
System Call: execv()
System Call: _exit()
System Call: kill_curthread()

## Configuration
### Installation
Detailed installation instructions for the base OS161 may be found here and here. If you are simply working with this repository, the following libraries will be required:
'''
bmake
libmpc
gmp
wget
'''

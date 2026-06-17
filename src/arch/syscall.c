#include "syscall.h"
#include <arch/cpu.h>

int64_t yield() {
    int64_t ret;
    __asm__ volatile("mov x0, %1\n"
                     "svc #0\n"
                     "mov %0, x0"
                     : "=r"(ret)
                     : "r"((uint64_t)SYSCALL_YIELD)
                     : "x0");
    return ret;
}

void exit(int64_t status) {
    __asm__ volatile("mov x0, %0\n"
                     "mov x1, %1\n"
                     "svc #0"
                     :
                     : "r"((uint64_t)SYSCALL_EXIT), "r"(status)
                     : "x0", "x1");
    halt();
}

pid_t getpid() {
    int64_t ret;
    __asm__ volatile("mov x0, %1\n"
                     "svc #0\n"
                     "mov %0, x0"
                     : "=r"(ret)
                     : "r"((uint64_t)SYSCALL_GETPID)
                     : "x0");
    return ret;
}

int64_t waitpid(pid_t pid) {
    int64_t ret;
    __asm__ volatile("mov x0, %1\n"
                     "mov x1, %2\n"
                     "svc #0\n"
                     "mov %0, x0"
                     : "=r"(ret)
                     : "r"((uint64_t)SYSCALL_WAITPID), "r"(pid)
                     : "x0", "x1");
    return ret;
}

pid_t fork() {
    int64_t ret;
    __asm__ volatile("mov x0, %1\n"
                     "svc #0\n"
                     "mov %0, x0"
                     : "=r"(ret)
                     : "r"((uint64_t)SYSCALL_FORK)
                     : "x0");
    return ret;
}

int64_t sleep(time_t seconds) {
    int64_t ret;
    __asm__ volatile("mov x0, %1\n"
                     "mov x1, %2\n"
                     "svc #0\n"
                     "mov %0, x0"
                     : "=r"(ret)
                     : "r"((uint64_t)SYSCALL_SLEEP), "r"(seconds)
                     : "x0", "x1");
    return ret;
}

int64_t msleep(mseconds_t ms) {
    int64_t ret;
    __asm__ volatile("mov x0, %1\n"
                     "mov x1, %2\n"
                     "svc #0\n"
                     "mov %0, x0"
                     : "=r"(ret)
                     : "r"((uint64_t)SYSCALL_MSLEEP), "r"(ms)
                     : "x0", "x1");
    return ret;
}

time_t time() {
    time_t ret;
    __asm__ volatile("mov x0, %1\n"
                     "svc #0\n"
                     "mov %0, x0"
                     : "=r"(ret)
                     : "r"((uint64_t)SYSCALL_TIME)
                     : "x0");
    return ret;
}

time_t uptime() {
    time_t ret;
    __asm__ volatile("mov x0, %1\n"
                     "svc #0\n"
                     "mov %0, x0"
                     : "=r"(ret)
                     : "r"((uint64_t)SYSCALL_UPTIME)
                     : "x0");
    return ret;
}
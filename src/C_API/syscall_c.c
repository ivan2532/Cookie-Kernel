/*
    C API wrapper to system calls

    In these functions we store the system call code and arguments in the
    corresponding registers and then switch to supervisor mode from which
    we call the internal kernel functions.
*/

#include "syscall_c.h"

// Allocate a memory block of "size" bytes on the heap.
void* __mem_alloc(size_t size)
{
    // Store arguments starting from A1
    __asm__ volatile ("mv a1, %[inSize]" : : [inSize] "r" (size));

    // Store the system call code in register A0
    __asm__ volatile ("li a0, 0x01");

    // Generate interrupt
    __asm__ volatile ("ecall");

    // Get the return value after ECALL
    void* returnValue = 0;
    __asm__ volatile ("mv %[outReturn], a0" : [outReturn] "=r" (returnValue));

    return returnValue;
}

// Free memory allocated by __mem_alloc
int __mem_free(void* ptr)
{
    // Store arguments starting from A1
    __asm__ volatile ("mv a1, %[inPtr]" : : [inPtr] "r" (ptr));

    // Store the system call code in register A0
    __asm__ volatile ("li a0, 0x02");

    // Generate interrupt
    __asm__ volatile ("ecall");

    // Get the return value after ECALL
    int returnValue = 0;
    __asm__ volatile ("mv %[outReturn], a0" : [outReturn] "=r" (returnValue));

    return returnValue;
}
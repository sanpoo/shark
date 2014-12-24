/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "env.h"
#include "coro_switch.h"

__asm__(
    ".text\n"
    ".globl context_switch\n"
    "context_switch:\n"
#if __amd64
    #define NUM_SAVED 6
    "push %rbp\n"
    "push %rbx\n"
    "push %r12\n"
    "push %r13\n"
    "push %r14\n"
    "push %r15\n"
    "mov %rsp, (%rdi)\n"
    "mov (%rsi), %rsp\n"
    "pop %r15\n"
    "pop %r14\n"
    "pop %r13\n"
    "pop %r12\n"
    "pop %rbx\n"
    "pop %rbp\n"
    "pop %rcx\n"
    "jmp *%rcx\n"
#elif __i386
    #define NUM_SAVED 4
    "push %ebp\n"
    "push %ebx\n"
    "push %esi\n"
    "push %edi\n"
    "mov %esp, (%eax)\n"
    "mov (%edx), %esp\n"
    "pop %edi\n"
    "pop %esi\n"
    "pop %ebx\n"
    "pop %ebp\n"
    "pop %ecx\n"
    "jmp *%ecx\n"
#else
    #error unsupported architecture
#endif
);

void coro_routine_entry();

__asm__(
    ".text\n"
    "coro_routine_entry:\n"
#if __amd64
    "pop %rdi\n"
    "pop %rcx\n"
    "call *%rcx\n"
#elif __i386
    "pop %eax\n"
    "pop %ecx\n"
    "call *%ecx\n"
#else
    #error unsupported architecture
#endif
);

void coro_stack_init(struct context *ctx, struct coro_stack *stack, coro_routine routine, void *args)
{
    ctx->sp = (void **)stack->ptr;
    *--ctx->sp = (void *)routine;
    *--ctx->sp = (void *)args;
    *--ctx->sp = (void *)coro_routine_entry;
    ctx->sp -= NUM_SAVED;
}

/*
    make sure size_bytes aligned by PAGE_SIZE
*/
int coro_stack_alloc(struct coro_stack *stack, size_t size_bytes)
{
    stack->size_bytes = size_bytes;
    size_bytes += PAGE_SIZE;

    stack->ptr = mmap(NULL, size_bytes, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack->ptr == (void *) - 1)
        return -1;

    mprotect(stack->ptr, PAGE_SIZE, PROT_NONE);
    stack->ptr = (void *)((char *)stack->ptr + size_bytes);

    return 0;
}

void coro_stack_free(struct coro_stack *stack)
{
    stack->ptr = (void *)((char *)stack->ptr - stack->size_bytes - PAGE_SIZE);
    munmap(stack->ptr, stack->size_bytes + PAGE_SIZE);
}



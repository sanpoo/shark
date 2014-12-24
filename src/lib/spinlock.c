/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <sched.h>

#include "pub.h"
#include "spinlock.h"

void spin_lock_init(spinlock *lock)
{
    lock->counter = 0;
}

int spin_trylock(spinlock *lock)
{
    return (lock->counter == 0 && __sync_bool_compare_and_swap(&lock->counter, 0, -1));
}

void spin_lock(spinlock *lock)
{
    for (;;)
    {
        if (lock->counter == 0 && __sync_bool_compare_and_swap(&lock->counter, 0, -1))
            return;

#ifdef CONFIG_SMP
        int n, i;
        for (n = 1; n < 2048; n <<= 1)
        {
            for (i = 0; i < n; i++)
                cpu_pause();

            if (lock->counter == 0 && __sync_bool_compare_and_swap(&lock->counter, 0, -1))
                return;
        }
#endif

        sched_yield();
    }
}

void spin_unlock(spinlock *lock)
{
    __sync_bool_compare_and_swap(&lock->counter, -1, 0);
}


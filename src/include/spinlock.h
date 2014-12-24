/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

typedef struct
{
	volatile int counter;
} spinlock;

void spin_lock_init(spinlock *lock);
int spin_trylock(spinlock *lock);
void spin_lock(spinlock *lock);
void spin_unlock(spinlock *lock);

#endif


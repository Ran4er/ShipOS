//
// Created by ShipOS developers on 03.01.24.
// Copyright (c) 2024 SHIPOS. All rights reserved.
//

#include "spinlock.h"
#include "../lib/include/panic.h"
#include <stddef.h>  // FIX: Добавлен этот include для NULL

void pushcli(void);
void popcli(void);

void init_spinlock(struct spinlock *lk, char *name) {
    lk->name = name;
    lk->is_locked = 0;
}

void acquire_spinlock(struct spinlock *lk) {
    if (lk == NULL) {
        panic("acquire_spinlock: null lock");
    }
    
    pushcli(); // disable interrupts
    
    while (xchg((volatile uint32_t*)&lk->is_locked, 1) != 0);
    
    __sync_synchronize();
}

void release_spinlock(struct spinlock *lk) {
    if (lk == NULL) {
        panic("release_spinlock: null lock");
    }
    
    __sync_synchronize();
    
    xchg((volatile uint32_t*)&lk->is_locked, 0);
    
    popcli();
}

int holding_spinlock(struct spinlock *lk) {
    return lk->is_locked;
}

void pushcli(void) {
    __asm__ volatile("cli");
}

void popcli(void) {
    __asm__ volatile("sti");
}

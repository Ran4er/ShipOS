//
// Created by ShipOS developers on 03.01.24.
// Copyright (c) 2024 SHIPOS. All rights reserved.
//

#include "mutex.h"
#include "../lib/include/panic.h"
#include "../sched/scheduler.h"  // Для yield()

// FIX: Переименовать bool в is_holding
int init_mutex(struct mutex *lk, char *name) {
    lk->spinlock = kalloc();
    lk->thread_list = NULL;
    if (lk->spinlock == NULL) {
        return -1;
    }
    init_spinlock(lk->spinlock, name);
    return 0;
}

void acquire_mutex(struct mutex *lk) {
    if (lk == NULL) {
        panic("acquire_mutex: null mutex");
    }
    
check_mutex:
    // FIX: Переменная не должна называться bool, так как bool - это тип
    int is_holding = holding_spinlock(lk->spinlock);
    if (is_holding == 0) {
        acquire_spinlock(lk->spinlock);
        return;
    } else {
        push_thread_list(&lk->thread_list, current_cpu.current_thread);
        change_thread_state(current_cpu.current_thread, WAIT);
        yield();
        if (holding_spinlock(lk->spinlock) != 0) {
            panic("acquire_mutex: spinlock not free");
        }
        goto check_mutex;
    }
}

void release_mutex(struct mutex *lk) {
    if (lk == NULL || lk->spinlock == NULL) {
        panic("release_mutex: null mutex");
    }
    if (lk->spinlock->is_locked == 0) {
        panic("release_mutex: not locked");
    }
    if (lk->thread_list == NULL) {
        release_spinlock(lk->spinlock);
    } else {
        struct thread *thread = pop_thread_list(&lk->thread_list);
        change_thread_state(thread, RUNNABLE);
    }
}

void destroy_mutex(struct mutex *lk) {
    if (lk == NULL) return;
    if (lk->spinlock) {
        kfree(lk->spinlock);
    }
    kfree(lk);
}

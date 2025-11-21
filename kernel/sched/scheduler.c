//
// Created by ShipOS developers on 28.10.23.
// Copyright (c) 2023 SHIPOS. All rights reserved.
//

#include "scheduler.h"
#include "proc.h"
#include "threads.h"
#include "../lib/include/x86_64.h"
#include "../lib/include/panic.h"

struct context kcontext;
struct context *kcontext_ptr = &kcontext;
uint32_t current_proc_rounds = 0;

// FIX: Добавлено возвращаемое значение во всех ветках
struct thread *get_next_thread() {
    if (proc_list == NULL) {
        panic("schedule: no procs");
        return NULL;  // FIX: Добавлен return
    }
    
    struct proc *first_proc = peek_proc_list(proc_list);
    struct proc *current_proc = first_proc;

    if (current_proc_rounds >= ROUNDS_PER_PROC) {
        current_proc_rounds = 0;
        shift_proc_list(&proc_list);
    }

    do {
        current_proc = peek_proc_list(proc_list);

        if (current_proc->threads == NULL) {
            panic("schedule: proc with no threads");
            return NULL;  // FIX: Добавлен return
        }

        struct thread *first_thread = peek_thread_list(current_proc->threads);
        struct thread *current_thread;

        do {
            current_thread = peek_thread_list(current_proc->threads);
            shift_thread_list(&(current_proc->threads));
            if (current_thread->state == RUNNABLE) {
                current_proc_rounds++;
                return current_thread;
            }
        } while (peek_thread_list(current_proc->threads) != first_thread);

        current_proc_rounds = 0;
        shift_proc_list(&proc_list);
    } while (peek_proc_list(proc_list) != first_proc);

    panic("schedule: no available threads");
    return NULL;  // FIX: Добавлен return
}

void scheduler() {
    while (1) {
        struct thread *next_thread = get_next_thread();
        if (next_thread == NULL) {
            panic("scheduler: no next thread");
        }
        current_cpu.current_thread = next_thread;
        switch_context(&kcontext_ptr, next_thread->context);
    }
}

void yield() {
    if (current_cpu.current_thread == NULL) {
        panic("yield: no current thread");
    }
    switch_context(&(current_cpu.current_thread->context), kcontext_ptr);
}

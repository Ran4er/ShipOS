//
// Created by ShipOS developers on 20.12.23.
// Copyright (c) 2023 SHIPOS. All rights reserved.
//

#include "threads.h"
#include "sched_states.h"
#include "../lib/include/panic.h"
#include "scheduler.h"

struct thread *current_thread = 0;

void init_thread(struct thread *thread, void (*start_function)(void *), int argc, struct argument *args) {
    // FIX: Убрать лишние приведения типов, stack/kstack теперь void*
    thread->stack = kalloc();
    thread->kstack = kalloc();
    
    if (thread->stack == NULL || thread->kstack == NULL) {
        panic("init_thread: kalloc failed");
    }
    
    // FIX: memset работает напрямую с void*
    memset(thread->stack, 0, PGSIZE);
    memset(thread->kstack, 0, PGSIZE);
    
    // FIX: Арифметика указателей теперь корректна
    char *sp = (char *)thread->stack + PGSIZE - 16;
    
    thread->start_function = start_function;
    thread->argc = argc;
    thread->args = args;
    
    // FIX: Корректное приведение типа
    *(void (**)(void *))sp = start_function;
    
    sp -= sizeof(struct context) - sizeof(void *);
    memset(sp, 0, sizeof(struct context) - sizeof(void *));
    thread->context = (struct context *)sp;
    
    // FIX: Приведение типов
    thread->context->rdi = (uint64_t)argc;
    thread->context->rsi = (uint64_t)args;
}

struct thread *create_thread(void (*start_function)(void *), int argc, struct argument *args) {
    struct thread *new_thread = (struct thread *)kalloc();
    if (new_thread == NULL) {
        return NULL;
    }
    init_thread(new_thread, start_function, argc, args);
    return new_thread;
}

void push_thread_list(struct thread_node **list, struct thread *thread) {
    struct thread_node *new_node = kalloc();
    if (new_node == NULL) {
        panic("push_thread_list: kalloc failed");
    }
    
    new_node->data = thread;
    if (*list != NULL) {
        new_node->next = *list;
        new_node->prev = (*list)->prev;
        (*list)->prev->next = new_node;
        (*list)->prev = new_node;
    } else {
        new_node->prev = new_node;
        new_node->next = new_node;
        *list = new_node;
    }
}

// FIX: Добавлено возвращаемое значение во всех ветках
struct thread *pop_thread_list(struct thread_node **list) {
    if (*list == NULL) {
        panic("pop_thread_list: empty list");
        return NULL;  // FIX: Добавлен return
    }
    
    struct thread *t = (*list)->data;
    struct thread_node *node_to_free = *list;
    
    // FIX: Исправлено = на == для сравнения
    if ((*list)->next == *list) {
        *list = NULL;
    } else {
        (*list)->prev->next = (*list)->next;
        (*list)->next->prev = (*list)->prev;
        *list = (*list)->next;
    }
    
    kfree(node_to_free);
    return t;
}

void shift_thread_list(struct thread_node **list) {
    if (*list == NULL) {
        panic("shift_thread_list: empty list");
    } else {
        *list = (*list)->next;
    }
}

// FIX: Добавлено возвращаемое значение во всех ветках
struct thread *peek_thread_list(struct thread_node *list) {
    if (list == NULL) {
        panic("peek_thread_list: empty list");
        return NULL;  // FIX: Добавлен return
    }
    return list->data;
}

void change_thread_state(struct thread *thread, enum sched_states new_state) {
    if (thread == NULL) {
        return;
    }
    thread->state = new_state;
}

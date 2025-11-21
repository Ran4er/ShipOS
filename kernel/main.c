//
// Created by ShipOS developers on 28.10.23.
// Copyright (c) 2023 SHIPOS. All rights reserved.
//

#include "vga/vga.h"
#include "idt/idt.h"
#include "tty/tty.h"
#include "kalloc/kalloc.h"
#include "memlayout.h"
#include "lib/include/x86_64.h"
#include "paging/paging.h"
#include "sched/proc.h"
#include "sched/threads.h"
#include "sched/scheduler.h"

// FIX: Удалена дублирующая thread_function (она уже в proc.c)

int kernel_main(){
    // FIX: Убран вызов несуществующей функции cls()
    init_tty();
    
    for (uint8_t i=0; i < TERMINALS_NUMBER; i++) {
        set_tty(i);
        printf("TTY %d\n", i);
    }
    set_tty(0);
    
    printf("=== ShipOS Kernel Boot ===\n");
    printf("CR3: %p\n", rcr3());

    printf("\nMemory layout:\n");
    printf("Kernel start: %p\n", KSTART);
    printf("Kernel end:   %p\n", KEND);
    printf("Kernel size:  %lu KB\n", ((uint64_t)KEND - KSTART) / 1024);

    // Инициализация buddy аллокатора на всей доступной памяти
    printf("\nInitializing buddy allocator...\n");
    kinit((uint64_t)KEND, PHYSTOP);
    
    // Отладочный вывод состояния памяти
    kalloc_dump();
    
    // Инициализация kernel page tables
    printf("\nInitializing kernel paging...\n");
    pagetable_t kernel_table = kvminit(KSTART, PHYSTOP);
    printf("Kernel page table at: %p\n", kernel_table);
    
    // Проверка, что paging работает
    printf("Successfully mapped physical memory:\n");
    printf("  %p - %p\n", KSTART, PHYSTOP);
    
    // Выводим свободную память
    uint64_t free_mem_mb = kalloc_free_memory() / (1024 * 1024);
    printf("Free memory: %lu MB\n", free_mem_mb);
    
    // Дальнейшая инициализация...
    printf("\nInitializing process system...\n");
    struct proc_node *init_proc_node = procinit();
    printf("Init proc node: %p\n", init_proc_node);
    
    struct thread *init_thread = peek_thread_list(init_proc_node->data->threads);
    printf("Init thread: %p\n", init_thread);

    printf("\nSetting up IDT...\n");
    setup_idt();
    
    printf("=== Kernel initialization complete ===\n");

    // scheduler(); // Раскомментируйте, когда будет готов

    // Тест аллокатора (можно удалить в продакшене)
    void *test = kmalloc(12345);
    printf("Test kmalloc(12345) = %p\n", test);
    void *test2 = krealloc(test, 54321);
    printf("Test krealloc(%p, 54321) = %p\n", test, test2);
    kfree(test2);

    while(1) {};
    return 0;
}

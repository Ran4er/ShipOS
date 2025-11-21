#ifndef KALLOC_H
#define KALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_ORDER       11              // До 8MB (2^11 страниц)
#define MIN_ORDER       0               // 4KB (2^0 страниц)
#define BLOCK_SIZE(order) ((1ULL << (order)) * PGSIZE)

// Структура для свободных блоков
struct free_block {
    struct free_block *next;
};

// Глобальные функции
void kinit(uint64_t start, uint64_t end);           // Инициализация
void *kalloc(void);                                 // Выделить страницу (4KB) - для совместимости
void kfree(void *pa);                               // Освободить страницу - для совместимости

// Расширенный API
void *kmalloc(size_t size);                         // Выделить блок произвольного размера
void *kzalloc(size_t size);                         // Выделить и обнулить
void kfree_sized(void *pa, size_t size);            // Освободить с явным размером
void *krealloc(void *ptr, size_t new_size);         // Изменить размер блока

// Отладка
void kalloc_dump(void);                             // Вывести состояние аллокатора
uint64_t kalloc_free_memory(void);                  // Получить количество свободной памяти

#endif // KALLOC_H

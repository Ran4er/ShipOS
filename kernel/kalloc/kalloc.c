#include "kalloc.h"
#include "../sync/spinlock.h"
#include "../memlayout.h"
#include "../lib/include/memset.h"
#include "../tty/tty.h"
#include "../lib/include/x86_64.h"

// FIX: Добавить missing includes
#include "../lib/include/panic.h"

// FIX: Реализация memcpy для freestanding environment
static void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

// Макросы для работы с заголовками блоков
#define BLOCK_HEADER_SIZE sizeof(int)
#define SET_BLOCK_ORDER(ptr, order) (*(int *)(ptr) = (order))
#define GET_BLOCK_ORDER(ptr) (*(int *)(ptr))

// Глобальное состояние buddy-аллокатора
static struct {
    struct spinlock lock;
    struct free_block *free_lists[MAX_ORDER];
    uint64_t start_addr;
    uint64_t end_addr;
    uint64_t total_pages;
    uint8_t *page_orders;
} buddy;

#define PAGE_ORDER_INDEX(pa) (((uint64_t)(pa) - buddy.start_addr) / PGSIZE)
#define IS_PAGE_VALID(pa) ((uint64_t)(pa) >= buddy.start_addr && (uint64_t)(pa) < buddy.end_addr)

static int size_to_order(size_t size) {
    int order = MIN_ORDER;
    size_t block_size = PGSIZE;
    size_t total_size = size + BLOCK_HEADER_SIZE;
    
    while (block_size < total_size && order < MAX_ORDER - 1) {
        block_size <<= 1;
        order++;
    }
    return order;
}

static uint64_t get_buddy_addr(uint64_t addr, int order) {
    return addr ^ (BLOCK_SIZE(order));
}

static bool is_block_free(uint64_t addr, int order) {
    int page_idx = PAGE_ORDER_INDEX(addr);
    int expected_pages = 1 << order;
    
    if (addr + BLOCK_SIZE(order) > buddy.end_addr) {
        return false;
    }
    
    for (int i = 0; i < expected_pages; i++) {
        uint8_t info = buddy.page_orders[page_idx + i];
        if (info & 0x80) return false;
        if ((info & 0x7F) != order) return false;
    }
    return true;
}

static void mark_block(uint64_t addr, int order, bool occupied) {
    int page_idx = PAGE_ORDER_INDEX(addr);
    int pages = 1 << order;
    
    for (int i = 0; i < pages; i++) {
        buddy.page_orders[page_idx + i] = order | (occupied ? 0x80 : 0);
    }
}

void kinit(uint64_t start, uint64_t end) {
    init_spinlock(&buddy.lock, "buddy");
    
    start = (start + PGSIZE - 1) & ~(PGSIZE - 1);
    end = end & ~(BLOCK_SIZE(MAX_ORDER - 1) - 1);
    
    buddy.start_addr = start;
    buddy.end_addr = end;
    buddy.total_pages = (end - start) / PGSIZE;
    
    uint64_t orders_size = buddy.total_pages * sizeof(uint8_t);
    orders_size = (orders_size + PGSIZE - 1) & ~(PGSIZE - 1);
    
    if (start + orders_size >= end) {
        // FIX: Исправить вызов panic - без форматирования
        printf("kinit: not enough memory for orders array\n");
        panic("kinit");
    }
    
    buddy.page_orders = (uint8_t *)start;
    start += orders_size;
    
    memset(buddy.page_orders, 0xFF, buddy.total_pages);
    
    for (int i = 0; i < MAX_ORDER; i++) {
        buddy.free_lists[i] = NULL;
    }
    
    uint64_t addr = start;
    uint64_t block_size = BLOCK_SIZE(MAX_ORDER - 1);
    
    acquire_spinlock(&buddy.lock);
    while (addr + block_size <= end) {
        struct free_block *block = (struct free_block *)addr;
        block->next = buddy.free_lists[MAX_ORDER - 1];
        buddy.free_lists[MAX_ORDER - 1] = block;
        mark_block(addr, MAX_ORDER - 1, false);
        addr += block_size;
    }
    release_spinlock(&buddy.lock);
    
    printf("Buddy allocator: %p - %p, %lu pages\n", start, end, buddy.total_pages);
}

static void *alloc_block(int order) {
    if (order >= MAX_ORDER) return NULL;
    
    acquire_spinlock(&buddy.lock);
    
    int current_order = order;
    while (current_order < MAX_ORDER && buddy.free_lists[current_order] == NULL) {
        current_order++;
    }
    
    if (current_order >= MAX_ORDER) {
        release_spinlock(&buddy.lock);
        return NULL;
    }
    
    struct free_block *block = buddy.free_lists[current_order];
    buddy.free_lists[current_order] = block->next;
    
    uint64_t block_addr = (uint64_t)block;
    
    while (current_order > order) {
        current_order--;
        uint64_t buddy_addr = get_buddy_addr(block_addr, current_order);
        struct free_block *buddy_block = (struct free_block *)buddy_addr;
        buddy_block->next = buddy.free_lists[current_order];
        buddy.free_lists[current_order] = buddy_block;
        mark_block(buddy_addr, current_order, false);
    }
    
    mark_block(block_addr, order, true);
    release_spinlock(&buddy.lock);
    
    SET_BLOCK_ORDER(block, order);
    return (void *)((uint64_t)block + BLOCK_HEADER_SIZE);
}

void *kalloc(void) {
    return kmalloc(PGSIZE);
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    int order = size_to_order(size);
    void *block = alloc_block(order);
    
    if (block) {
        memset(block, 0, size);
    }
    
    return block;
}

void *kzalloc(size_t size) {
    return kmalloc(size);
}

static void free_block(void *ptr, int order) {
    if (!ptr) return;
    
    uint64_t block_addr = (uint64_t)ptr - BLOCK_HEADER_SIZE;
    
    if (!IS_PAGE_VALID(block_addr)) {
        // FIX: Исправить вызов panic
        printf("free_block: invalid address %p\n", ptr);
        panic("free_block");
    }
    
    acquire_spinlock(&buddy.lock);
    mark_block(block_addr, order, false);
    
    while (order < MAX_ORDER - 1) {
        uint64_t buddy_addr = get_buddy_addr(block_addr, order);
        
        if (!is_block_free(buddy_addr, order)) {
            break;
        }
        
        struct free_block **prev = &buddy.free_lists[order];
        while (*prev && (uint64_t)*prev != buddy_addr) {
            prev = &(*prev)->next;
        }
        
        if (!*prev) {
            break;
        }
        
        *prev = (*prev)->next;
        
        if (buddy_addr < block_addr) {
            block_addr = buddy_addr;
        }
        order++;
    }
    
    struct free_block *block = (struct free_block *)block_addr;
    block->next = buddy.free_lists[order];
    buddy.free_lists[order] = block;
    
    release_spinlock(&buddy.lock);
}

void kfree(void *pa) {
    if (!pa) return;
    
    uint64_t block_addr = (uint64_t)pa - BLOCK_HEADER_SIZE;
    int page_idx = PAGE_ORDER_INDEX(block_addr);
    int order = buddy.page_orders[page_idx] & 0x7F;
    
    if (order == 0xFF || !(buddy.page_orders[page_idx] & 0x80)) {
        printf("kfree: invalid free %p\n", pa);
        panic("kfree");
    }
    
    free_block(pa, order);
}

void kfree_sized(void *pa, size_t size) {
    if (!pa) return;
    int expected_order = size_to_order(size);
    free_block(pa, expected_order);
}

void *krealloc(void *ptr, size_t new_size) {
    if (ptr == NULL) {
        return kmalloc(new_size);
    }
    
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    uint64_t block_addr = (uint64_t)ptr - BLOCK_HEADER_SIZE;
    if (!IS_PAGE_VALID(block_addr)) {
        printf("krealloc: invalid pointer %p\n", ptr);
        panic("krealloc");
    }
    
    int page_idx = PAGE_ORDER_INDEX(block_addr);
    int current_order = buddy.page_orders[page_idx] & 0x7F;
    
    if (current_order == 0xFF || !(buddy.page_orders[page_idx] & 0x80)) {
        printf("krealloc: invalid pointer %p\n", ptr);
        panic("krealloc");
    }
    
    size_t current_size = BLOCK_SIZE(current_order) - BLOCK_HEADER_SIZE;
    
    if (new_size <= current_size) {
        return ptr;
    }
    
    void *new_ptr = kmalloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    memcpy(new_ptr, ptr, current_size);
    free_block(ptr, current_order);
    
    return new_ptr;
}

void kalloc_dump(void) {
    acquire_spinlock(&buddy.lock);
    
    printf("Buddy allocator state:\n");
    printf("Range: %p - %p\n", buddy.start_addr, buddy.end_addr);
    
    uint64_t total_free = 0;
    for (int order = 0; order < MAX_ORDER; order++) {
        int count = 0;
        struct free_block *block = buddy.free_lists[order];
        while (block) {
            count++;
            block = block->next;
        }
        
        if (count > 0) {
            uint64_t bytes = count * BLOCK_SIZE(order);
            total_free += bytes;
            printf("  Order %d (%6lu KB): %4d blocks, %6lu KB\n", 
                   order, BLOCK_SIZE(order) / 1024, count, bytes / 1024);
        }
    }
    
    printf("Total free: %lu KB (%lu MB)\n", total_free / 1024, total_free / (1024 * 1024));
    release_spinlock(&buddy.lock);
}

uint64_t kalloc_free_memory(void) {
    acquire_spinlock(&buddy.lock);
    uint64_t total = 0;
    for (int order = 0; order < MAX_ORDER; order++) {
        struct free_block *block = buddy.free_lists[order];
        while (block) {
            total += BLOCK_SIZE(order);
            block = block->next;
        }
    }
    release_spinlock(&buddy.lock);
    return total;
}

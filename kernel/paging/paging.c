//
// Created by ShipOS developers on 28.10.23.
// Copyright (c) 2023 SHIPOS. All rights reserved.
//

#include "paging.h"
#include "../tty/tty.h"
#include "../kalloc/kalloc.h"
#include "../lib/include/memset.h"
#include "../memlayout.h"
#include "../lib/include/x86_64.h"
#include "../lib/include/panic.h"

page_entry_raw encode_page_entry(struct page_entry entry) {
    page_entry_raw raw = 0;

    raw |= (entry.p & 0x1);
    raw |= (entry.rw & 0x1) << 1;
    raw |= (entry.us & 0x1) << 2;
    raw |= (entry.pwt & 0x1) << 3;
    raw |= (entry.pcd & 0x1) << 4;
    raw |= (entry.a & 0x1) << 5;
    raw |= (entry.d & 0x1) << 6;
    raw |= (entry.rsvd & 0x1) << 7;
    raw |= (entry.ign1 & 0xF) << 8;
    raw |= (entry.address & 0xFFFFFFFFF) << 12;
    raw |= (uint64_t)(entry.ign2 & 0x7FFF) << 48;
    raw |= (uint64_t)(entry.xd & 0x1) << 63;

    return raw;
}

struct page_entry decode_page_entry(page_entry_raw raw) {
    struct page_entry entry;

    entry.p = raw & 0x1;
    entry.rw = (raw >> 1) & 0x1;
    entry.us = (raw >> 2) & 0x1;
    entry.pwt = (raw >> 3) & 0x1;
    entry.pcd = (raw >> 4) & 0x1;
    entry.a = (raw >> 5) & 0x1;
    entry.d = (raw >> 6) & 0x1;
    entry.rsvd = (raw >> 7) & 0x1;
    entry.ign1 = (raw >> 8) & 0xF;
    entry.address = (raw >> 12) & 0xFFFFFFFFF;
    entry.ign2 = (raw >> 48) & 0x7FFF;
    entry.xd = (raw >> 63) & 0x1;

    return entry;
}

void print_entry(struct page_entry *entry) {
    printf("P: %d RW: %d US: %d PWT: %d A: %d D: %d ADDR: %p\n", 
           entry->p, entry->rw, entry->us, entry->pwt, entry->a, entry->d, entry->address << 12);
}

void do_print_vm(pagetable_t tbl, int level) {
    int spaces = 4 - level + 1;
    for (size_t i = 0; i < 512; i++) {
        struct page_entry entry = decode_page_entry(tbl[i]);
        if (entry.p) {
            for (int j = 0; j < spaces; j++) {
                print(".. ");
            }
            print_entry(&entry);
            if (level > 1) do_print_vm((pagetable_t)(entry.address << 12), level-1);
        }
    }
}

void print_vm(pagetable_t tbl) {
    do_print_vm(tbl, 4);
}

// Initialize a page table entry for page mapping
void init_entry(page_entry_raw *raw_entry, uint64_t addr) {
    struct page_entry entry;

    entry.p = 1;    
    entry.rw = 1;
    entry.us = 0;
    entry.pwt = 0;
    entry.pcd = 0;
    entry.a = 0;
    entry.d = 0;
    entry.rsvd = 1; // 1 = points to page
    entry.ign1 = 0;
    entry.address = (addr >> 12) & 0xFFFFFFFFF;
    entry.ign2 = 0;
    entry.xd = 0;

    *raw_entry = encode_page_entry(entry);
}

// Initialize a page table entry for next-level table
void init_table_entry(page_entry_raw *raw_entry, uint64_t addr) {
    struct page_entry entry;

    entry.p = 1;    
    entry.rw = 1;
    entry.us = 0;
    entry.pwt = 0;
    entry.pcd = 0;
    entry.a = 0;
    entry.d = 0;
    entry.rsvd = 0; // 0 = points to table
    entry.ign1 = 0;
    entry.address = (addr >> 12) & 0xFFFFFFFFF;
    entry.ign2 = 0;
    entry.xd = 0;

    *raw_entry = encode_page_entry(entry);
}

// Walk page tables and return entry for virtual address
// FIX: Теперь сигнатура совпадает с заголовком
page_entry_raw *walk(pagetable_t tbl, uint64_t va, bool alloc) {
    for (int level = 3; level > 0; level--) {
        int level_index = (va >> (12 + level * 9)) & 0x1FF;
        page_entry_raw *entry_raw = &tbl[level_index];
        struct page_entry entry = decode_page_entry(*entry_raw);

        if (entry.p) {
            // Entry already exists, follow it
            tbl = (pagetable_t)(entry.address << 12);
        } else {
            // Need to allocate new table
            if (alloc == 0 || (tbl = kalloc()) == NULL) {
                return NULL;
            }
            memset(tbl, 0, PGSIZE);
            init_table_entry(entry_raw, (uint64_t)tbl);
        }
    }

    return &tbl[(va >> 12) & 0x1FF];
}

// Initialize kernel page table for range [start, end)
pagetable_t kvminit(uint64_t start, uint64_t end) {
    pagetable_t tbl4 = (pagetable_t)rcr3();
    
    // Map all physical memory from start to end
    for (uint64_t addr = PGROUNDUP(start); addr < end; addr += PGSIZE) {
        page_entry_raw *entry_raw = walk(tbl4, addr, 1);
        if (entry_raw == NULL) {
            printf("kvminit: out of memory while mapping %p\n", addr);
            panic("kvminit");
        }
        init_entry(entry_raw, addr);
    }

    return tbl4;
}

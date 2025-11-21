//
// Created by ShipOS developers on 28.10.23.
// Copyright (c) 2023 SHIPOS. All rights reserved.
//

#include "../include/panic.h"
#include "../../tty/tty.h"  // Для printf

void panic(const char *message) {
    printf("PANIC: %s\n", message);
    __asm__ volatile("cli; hlt");
    while(1) {}
}

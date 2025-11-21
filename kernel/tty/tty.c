//
// Created by ShipOS developers on 28.10.23.
// Copyright (c) 2023 SHIPOS. All rights reserved.
//

#include <stdarg.h>
#include <stddef.h>  // Для NULL
#include "tty.h"
#include <inttypes.h>
#include "../lib/include/memset.h"
#include "../lib/include/panic.h"

#define TERMINALS_NUMBER 7
static tty_structure tty_terminals[TERMINALS_NUMBER];
static tty_structure *active_tty;
static struct spinlock printf_spinlock;
static struct spinlock print_spinlock;

// Форвард-объявления вспомогательных функций
static void scroll(void);
static void write_char_to_buffer(char ch);
static void update_cursor(void);

void set_fg(enum vga_colors _fg) {
    active_tty->fg = _fg;
}

void set_bg(enum vga_colors _bg) {
    active_tty->bg = _bg;
}

void init_tty() {
    memset(&tty_terminals, 0, sizeof(tty_structure) * TERMINALS_NUMBER);
    for (int i = 0; i < TERMINALS_NUMBER; ++i) {
        (tty_terminals + i)->tty_id = i;
        (tty_terminals + i)->bg = DEFAULT_BG_COLOR;
        (tty_terminals + i)->fg = DEFAULT_FG_COLOR;
        (tty_terminals + i)->line = (tty_terminals + i)->pos = 0;
        memset((tty_terminals + i)->tty_buffer, 0, VGA_HEIGHT * VGA_WIDTH);
    }
    set_tty(0);
    init_spinlock(&printf_spinlock, "printf spinlock");
    init_spinlock(&print_spinlock, "print spinlock");
}

void set_tty(uint8_t terminal) {
    if (TERMINALS_NUMBER <= terminal) {
        return;
    }
    clear_vga();
    active_tty = tty_terminals + terminal;
    write_buffer(active_tty->tty_buffer);
}

void clear_current_tty() {
    memset(active_tty->tty_buffer, 0, VGA_WIDTH * VGA_HEIGHT);
    active_tty->pos = 0;
    active_tty->line = 0;
    clear_vga();
}

uint8_t get_current_tty() {
    return active_tty->tty_id;
}

void reverse(char *str, int n) {
    int i = 0;
    int j = n - 1;
    while (i < j) {
        char tmp = str[i];
        str[i++] = str[j];
        str[j--] = tmp;
    }
}

struct char_with_color make_char(char value, enum vga_colors fg, enum vga_colors bg) {
    struct char_with_color res = {
            .character = value,
            .color = fg + (bg << 4)
    };
    return res;
}

static void scroll() {
    for (int i = 1; i < VGA_HEIGHT; i++) {
        for (int j = 0; j < VGA_WIDTH; j++) {
            active_tty->tty_buffer[(i - 1) * VGA_WIDTH + j] = active_tty->tty_buffer[i * VGA_WIDTH + j];
        }
    }
    for (int i = 0; i < VGA_WIDTH; i++) {
        active_tty->tty_buffer[VGA_WIDTH * (VGA_HEIGHT - 1) + i] = make_char(0, 0, 0);
    }
    active_tty->line = VGA_HEIGHT - 1;
    active_tty->pos = 0;
}

// FIX: putchar теперь принимает char, а не const char*
void putchar(char ch) {
    if (ch == '\n') {
        active_tty->line++;
        active_tty->pos = 0;
        if (active_tty->line >= VGA_HEIGHT) {
            scroll();
        }
    } else if (ch == '\r') {
        active_tty->pos = 0;
    } else if (ch == '\t') {
        int spaces = 8 - (active_tty->pos % 8);
        for (int i = 0; i < spaces; i++) {
            putchar(' ');
        }
    } else if (ch == '\b') {
        if (active_tty->pos > 0) {
            active_tty->pos--;
            write_char_to_buffer(' ');
        }
    } else {
        write_char_to_buffer(ch);
        active_tty->pos++;
        if (active_tty->pos >= VGA_WIDTH) {
            active_tty->pos = 0;
            active_tty->line++;
            if (active_tty->line >= VGA_HEIGHT) {
                scroll();
            }
        }
    }
    update_cursor();
}

static void write_char_to_buffer(char ch) {
    int index = active_tty->line * VGA_WIDTH + active_tty->pos;
    active_tty->tty_buffer[index] = make_char(ch, active_tty->fg, active_tty->bg);
}

static void update_cursor(void) {
    uint16_t pos = active_tty->line * VGA_WIDTH + active_tty->pos;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void print(const char *string) {
    if (string == NULL) return;
    acquire_spinlock(&print_spinlock);
    while (*string != 0) {
        putchar(*string++);  // FIX: Передаем символ, а не указатель
    }
    write_buffer(active_tty->tty_buffer);
    release_spinlock(&print_spinlock);
}

void itoa(int num, char *str, int radix) {
    int i = 0;
    int is_negative = 0;
    if (num < 0 && radix != 16) {
        is_negative = 1;
        num *= -1;
    }

    do {
        int rem = (num % radix);
        str[i++] = (rem > 9 ? 'a' - 10 : '0') + rem;
        num /= radix;
    } while (num);

    if (is_negative) str[i++] = '-';
    reverse(str, i);
    str[i] = 0;
}

void ptoa(uint64_t num, char *str) {
    int i = 0;

    do {
        int rem = (num % 16);
        str[i++] = (rem > 9 ? 'a' - 10 : '0') + rem;
        num /= 16;
    } while (num);

    reverse(str, i);
    str[i] = 0;
}

void printf(const char *format, ...) {
    if (format == NULL) return;
    
    acquire_spinlock(&printf_spinlock);
    va_list varargs;
    va_start(varargs, format);
    char digits_buf[100];
    memset(digits_buf, 0, 100);
    
    while (*format) {
        if (*format == '%') {
            format++;
            if (*format == '\0') break;
            
            switch (*format) {
                case 'd':
                    itoa(va_arg(varargs, int), digits_buf, 10);
                    print(digits_buf);
                    break;
                case 'o':
                    itoa(va_arg(varargs, int), digits_buf, 8);
                    print(digits_buf);
                    break;
                case 'x':
                    itoa(va_arg(varargs, int), digits_buf, 16);
                    print(digits_buf);
                    break;
                case 'b':
                    itoa(va_arg(varargs, int), digits_buf, 2);
                    print(digits_buf);
                    break;
                case 'p':
                    ptoa(va_arg(varargs, uint64_t), digits_buf);
                    print(digits_buf);
                    break;
                case 's':
                    print(va_arg(varargs, char*));
                    break;
                case '%':
                    putchar('%');
                    break;
                default:
                    putchar('#');
            }
        } else {
            putchar(*format);  // FIX: Передаем символ, а не указатель
            write_buffer(active_tty->tty_buffer);
        }
        format++;
    }
    va_end(varargs);
    release_spinlock(&printf_spinlock);
}

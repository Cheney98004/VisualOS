#include "terminal.h"

#include <stdarg.h>

// Basic VGA text mode terminal with scrolling and a tiny printf helper.
static volatile uint16_t *const vga_buffer = (uint16_t *)0xB8000;
static uint8_t term_row;
static uint8_t term_col;
static uint8_t term_color;

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)color << 8 | (uint8_t)c;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void set_cursor(void) {
    uint16_t pos = (uint16_t)(term_row * VGA_WIDTH + term_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void terminal_clear(void) {
    const uint16_t blank = vga_entry(' ', term_color);
    for (size_t y = 0; y < VGA_HEIGHT; ++y) {
        for (size_t x = 0; x < VGA_WIDTH; ++x) {
            vga_buffer[y * VGA_WIDTH + x] = blank;
        }
    }
    term_row = 0;
    term_col = 0;
    set_cursor();
}

void terminal_set_color(uint8_t fg, uint8_t bg) {
    term_color = vga_entry_color(fg, bg);
}

void terminal_init(void) {
    terminal_set_color(7, 0); // light grey on black
    terminal_clear();
}

static void scroll_if_needed(void) {
    if (term_row < VGA_HEIGHT) {
        return;
    }

    // Shift lines up by one.
    for (size_t y = 1; y < VGA_HEIGHT; ++y) {
        for (size_t x = 0; x < VGA_WIDTH; ++x) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        }
    }

    // Clear last line.
    const uint16_t blank = vga_entry(' ', term_color);
    for (size_t x = 0; x < VGA_WIDTH; ++x) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }

    term_row = VGA_HEIGHT - 1;
}

void terminal_putc(char c) {
    if (c == '\n') {
        term_col = 0;
        ++term_row;
        scroll_if_needed();
        set_cursor();
        return;
    }

    if (c == '\b') {
        if (term_col > 0) {
            --term_col;
        } else if (term_row > 0) {
            --term_row;
            term_col = VGA_WIDTH - 1;
        }
        vga_buffer[term_row * VGA_WIDTH + term_col] = vga_entry(' ', term_color);
        set_cursor();
        return;
    }

    vga_buffer[term_row * VGA_WIDTH + term_col] = vga_entry(c, term_color);
    ++term_col;
    if (term_col >= VGA_WIDTH) {
        term_col = 0;
        ++term_row;
        scroll_if_needed();
    }
    set_cursor();
}

void terminal_write(const char *s) {
    while (*s) {
        terminal_putc(*s++);
    }
}

void terminal_write_line(const char *s) {
    terminal_write(s);
    terminal_putc('\n');
}

static void write_hex(uint32_t v) {
    static const char hex_digits[] = "0123456789ABCDEF";
    terminal_write("0x");
    for (int i = 7; i >= 0; --i) {
        uint8_t nibble = (uint8_t)((v >> (i * 4)) & 0xF);
        terminal_putc(hex_digits[nibble]);
    }
}

static void write_uint(uint32_t v) {
    char buf[11]; // max 4294967295
    int idx = 0;
    if (v == 0) {
        terminal_putc('0');
        return;
    }
    while (v > 0 && idx < (int)sizeof(buf)) {
        buf[idx++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (idx-- > 0) {
        terminal_putc(buf[idx]);
    }
}

void terminal_write_hex(uint32_t value) {
    write_hex(value);
}

void terminal_write_dec(int value) {
    if (value < 0) {
        terminal_putc('-');
        write_uint((uint32_t)(-value));
    } else {
        write_uint((uint32_t)value);
    }
}

void terminal_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            terminal_putc(*fmt++);
            continue;
        }
        ++fmt;
        char spec = *fmt++;
        switch (spec) {
            case 's': {
                const char *str = va_arg(args, const char *);
                terminal_write(str ? str : "(null)");
                break;
            }
            case 'c': {
                int ch = va_arg(args, int);
                terminal_putc((char)ch);
                break;
            }
            case 'x': {
                uint32_t v = va_arg(args, uint32_t);
                write_hex(v);
                break;
            }
            case 'd': {
                int v = va_arg(args, int);
                terminal_write_dec(v);
                break;
            }
            case 'u': {
                uint32_t v = va_arg(args, uint32_t);
                write_uint(v);
                break;
            }
            case '%':
                terminal_putc('%');
                break;
            default:
                terminal_putc('%');
                terminal_putc(spec);
                break;
        }
    }

    va_end(args);
}

void terminal_error() {
    uint8_t old_color = term_color;
    terminal_set_color(12, 0);
    terminal_write("[ERROR] ");
    terminal_set_color(old_color & 0x0F, old_color >> 4);
}
#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void terminal_init(void);
void terminal_clear(void);
void terminal_set_color(uint8_t fg, uint8_t bg);
void terminal_putc(char c);
void terminal_write(const char *s);
void terminal_write_line(const char *s);
void terminal_write_hex(uint32_t value);
void terminal_write_dec(int value);
void terminal_printf(const char *fmt, ...);
void terminal_error();

#endif /* TERMINAL_H */

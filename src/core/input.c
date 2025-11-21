#include "input.h"
#include "keyboard.h"
#include "terminal.h"

// Reads a line into buffer (null-terminated). Returns length (excluding null).
size_t input_read_line(char *buffer, size_t max_len) {
    if (max_len == 0) {
        return 0;
    }
    size_t idx = 0;
    for (;;) {
        char c = keyboard_read_char();
        if (c == '\r') {
            // Some keyboards send CR; normalize to LF
            c = '\n';
        }

        if (c == '\b') {
            if (idx > 0) {
                --idx;
                // Erase char visually
                terminal_putc('\b');
                terminal_putc(' ');
                terminal_putc('\b');
            }
            continue;
        }

        if (c == '\n') {
            terminal_putc('\n');
            buffer[idx] = '\0';
            return idx;
        }

        if (idx + 1 < max_len) {
            buffer[idx++] = c;
            terminal_putc(c);
        }
    }
}

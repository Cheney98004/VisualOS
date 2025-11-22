#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
char keyboard_read_char(void);
void keyboard_flush_buffer(void);

#endif /* KEYBOARD_H */

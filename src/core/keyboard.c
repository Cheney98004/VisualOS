#include "keyboard.h"

// Simple PS/2 keyboard polling driver using set 1 scancodes. Handles Enter and Backspace.
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static int shift_active;

// Scancode set 1 maps for main keys.
static const char keymap[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', // 0x0E Backspace
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',     // 0x1C Enter
    0,  // Control
    'a','s','d','f','g','h','j','k','l',';','\'', '`',
    0,  // Left Shift
    '\\','z','x','c','v','b','n','m',',','.','/',
    0,  // Right Shift
    '*',
    0,  // Alt
    ' ',
    0,  // CapsLock and rest unused here
};

static const char keymap_shift[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,
    'A','S','D','F','G','H','J','K','L',':','"','~',
    0,
    '|','Z','X','C','V','B','N','M','<','>','?',
    0,
    '*',
    0,
    ' ',
    0,
};

void keyboard_init(void) {
    shift_active = 0;
}

char keyboard_read_char(void) {
    for (;;) {
        // Wait for data in buffer
        while ((inb(0x64) & 1) == 0) {
            // spin
        }
        uint8_t sc = inb(0x60);

        // Check release
        if (sc & 0x80) {
            uint8_t make = sc & 0x7F;
            if (make == 0x2A || make == 0x36) {
                shift_active = 0;
            }
            continue;
        }

        if (sc == 0x2A || sc == 0x36) { // shift
            shift_active = 1;
            continue;
        }

        char ch = shift_active ? keymap_shift[sc] : keymap[sc];
        if (ch == 0) {
            continue; // ignore unsupported keys
        }
        return ch;
    }
}

#include "terminal.h"
#include "keyboard.h"
#include "shell.h"
#include "ide.h"
#include "fat16.h"

extern uint8_t _data_vma[];
extern uint8_t _data_lma[];
extern uint8_t _edata[];
extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

static void memory_init() {
    uint8_t *src = _data_lma;
    uint8_t *dst = _data_vma;

    while (dst < _edata)
        *dst++ = *src++;

    dst = _bss_start;
    while (dst < _bss_end)
        *dst++ = 0;
}

__attribute__((section(".text.kmain")))
void kmain(void) {
    memory_init();
    terminal_init();
    keyboard_init();
    keyboard_flush_buffer();

    terminal_clear();
    terminal_write_line("VisualOS v0.4 (FAT16 Mode)");
    terminal_write_line("");

    // ----------------------------------------------------
    // Initialize disk and FAT16
    // ----------------------------------------------------
    if (!fat16_init()) {
        terminal_write_line("FATAL: FAT16 init failed!");
        for(;;);
    }

    terminal_write_line("FAT16 filesystem ready");
    terminal_write_line("");

    // ----------------------------------------------------
    // Test reading sector 0
    // ----------------------------------------------------
    uint8_t buf[512];
    ide_read_sector(0, buf);
    terminal_write_line("BOOT sector read OK");
    terminal_write_line("");

    fat16_protect_kernel();

    // ----------------------------------------------------
    // Enter shell
    // ----------------------------------------------------
    shell_run();

    for(;;);
}

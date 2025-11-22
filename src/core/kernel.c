#include "fs.h"
#include "keyboard.h"
#include "shell.h"
#include "terminal.h"
#include "pmm.h"

extern uint8_t _data_vma[];
extern uint8_t _data_lma[];
extern uint8_t _edata[];
extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

void memory_init() {
    uint8_t *src = _data_lma;
    uint8_t *dst = _data_vma;

    /* copy .data */
    while (dst < _edata)
        *dst++ = *src++;

    /* clear .bss */
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
    fs_init();

    terminal_clear();
    terminal_write_line("VisualOS ver.0.2");
    terminal_write_line("Linux-style RAM FS + basic shell");
    terminal_write_line("");

    pmm_init(32 * 1024 * 1024); 

    const FsNode *root = fs_root();
    terminal_write_line("Root listing:");
    fs_list(root);
    terminal_write_line("");

    shell_run();
}

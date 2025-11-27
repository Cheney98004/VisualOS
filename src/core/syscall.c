#include "syscall.h"
#include "terminal.h"
#include "keyboard.h"
#include "fs.h"

static void syscall_exit() {}

void syscall_init() {
    Syscalls *t = (Syscalls*)SYSCALL_TABLE_ADDR;

    t->print = terminal_write;
    t->read_key = keyboard_read_char;
    t->terminal_clear = terminal_clear;

    t->list_files = fs_list;
    t->create_file = fs_create;

    t->exit = syscall_exit;
}
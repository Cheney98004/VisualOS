#pragma once

typedef struct {
    void (*print)(const char*);
    int  (*read_key)();
    void (*terminal_clear)();
    
    void (*exit)();
} Syscalls;

#define SYSCALL_TABLE_ADDR 0x200000
#define syscalls ((Syscalls*)SYSCALL_TABLE_ADDR)

void syscall_init();
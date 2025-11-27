#pragma once

typedef struct {
    // Terminal
    void (*print)(const char*);
    int  (*read_key)();
    void (*terminal_clear)();
    
    // File System
    void (*list_files)(int);
    int (*create_file)(const char*);

    void (*exit)();
} Syscalls;

#define SYSCALL_TABLE_ADDR 0x200000
#define syscalls ((Syscalls*)SYSCALL_TABLE_ADDR)

void syscall_init();
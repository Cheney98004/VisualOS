#include "syscall.h"

void _start(void) {
    syscalls->create_file("test.txt");
}
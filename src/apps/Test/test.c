#include "syscall.h"

void _start(void) {
    syscalls->print("Test\n");
}
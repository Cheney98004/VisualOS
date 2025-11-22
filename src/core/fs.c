#include "fs.h"
#include "fat16.h"
#include "terminal.h"
#include "string.h"

void fs_init() {
    // nothing here; FAT16 init done in kernel
}

void fs_list() {
    char names[64][13];
    int n = fat16_list_dir(0, names, 64);

    for (int i = 0; i < n; i++) {
        terminal_write_line(names[i]);
    }
}

int fs_exists(const char *name) {
    char name83[11];
    fat16_format_83(name83, name);
    return fat16_find_in_dir(0, name83) >= 0;
}

int fs_delete(const char *name) {
    return fat16_delete(name);
}

int fs_create(const char *name) {
    return fat16_create_file(name);
}

int fs_read(const char *name, void *buf, int maxSize) {
    return fat16_read_file(name, buf, maxSize);
}

int fs_write(const char *name, const void *buf, int size) {
    return fat16_write_file(name, buf, size);
}

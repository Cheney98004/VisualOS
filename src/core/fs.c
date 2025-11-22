#include "fs.h"
#include "fat16.h"
#include "terminal.h"
#include "string.h"

// Track which directory we're in
static uint16_t currentDir = 0;   // 0 = root

void fs_init() {
    currentDir = 0;
}

uint16_t fs_current_dir_cluster() {
    return currentDir;
}

void fs_list() {
    char names[64][13];
    int n = fat16_list_dir(fat16_get_cwd(), names, 64);

    for (int i = 0; i < n; i++)
        terminal_write_line(names[i]);
}

int fs_exists(const char *name) {
    char name83[11];
    fat16_format_83(name83, name);
    return fat16_find_in_dir(currentDir, name) >= 0; 
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

int fs_mkdir(const char *name) {
    return fat16_create_directory(name);
}

int fs_cd(const char *dirname) {
    // 支援 root
    if (dirname[0] == '/' && dirname[1] == 0) {
        fat16_set_cwd(0);
        return 1;
    }

    // 直接使用真正的 FAT16 cd
    return fat16_cd(dirname);
}
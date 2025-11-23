#include "fs.h"
#include "fat16.h"
#include "terminal.h"
#include "string.h"

void fs_init() {
    fat16_set_cwd(0);
}

uint16_t fs_current_dir_cluster() {
    return fat16_get_cwd();
}

void fs_list(int printHideFiles) {
    Fat16DirEntry entries[256];
    int n = fat16_load_directory(fs_current_dir_cluster(), entries, 256);

    for (int i = 0; i < n; i++) {
        Fat16DirEntry *e = &entries[i];

        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5) continue;
        if (e->attr == FAT16_ATTR_LFN) continue;

        char type = (e->attr & FAT16_ATTR_DIRECTORY) ? 'd' : '-';

        uint8_t mode = e->flags;

        char name[13];
        fat16_decode_name(name, e->name);

        if ((mode & PERM_H) && !printHideFiles) continue;
        terminal_write(name);
        terminal_putc('\n');
    }
}

void fs_list_long(int printHideFiles) {
    Fat16DirEntry entries[256];
    int n = fat16_load_directory(fs_current_dir_cluster(), entries, 256);

    for (int i = 0; i < n; i++) {
        Fat16DirEntry *e = &entries[i];

        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5) continue;
        if (e->attr == FAT16_ATTR_LFN) continue;

        // type
        char type = (e->attr & FAT16_ATTR_DIRECTORY) ? 'd' : '-';

        // permissions (from flags)
        uint8_t mode = e->flags;

        char r = (mode & PERM_R) ? 'r' : '-';
        char w = (mode & PERM_W) ? 'w' : '-';
        char x = (mode & PERM_X) ? 'x' : '-';

        // Special states: hidden, system, immutable
        char h = (mode & PERM_H) ? 'h' : '-';
        char s = (mode & PERM_S) ? 's' : '-';
        char m = (mode & PERM_I) ? 'i' : '-';

        char name[13];
        fat16_decode_name(name, e->name);

        if ((mode & PERM_H) && !printHideFiles) continue;

        terminal_putc(type);
        terminal_putc(r);
        terminal_putc(w);
        terminal_putc(x);
        terminal_putc(' ');
        terminal_putc(h);
        terminal_putc(s);
        terminal_putc(m);
        terminal_putc(' ');
        terminal_write(name);
        terminal_putc('\n');
    }
}

int fs_exists(const char *name) {
    return fat16_find_in_dir(fat16_get_cwd(), name) >= 0;
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
    if (dirname[0] == '/' && dirname[1] == 0) {
        fat16_set_cwd(0);
        return 1;
    }

    return fat16_cd(dirname);
}

int fs_rename(const char *oldDirName, const char *newDirName) {
    return fat16_rename(oldDirName, newDirName);
}

int fs_get_entry(const char *name, Fat16DirEntry *outEntry, uint32_t *outLBA, int *outIndex) {
    uint16_t cwd = fs_current_dir_cluster();

    char name83[11];
    fat16_format_83(name83, name);

    Fat16DirEntry entries[256];
    int total = fat16_load_directory(cwd, entries, 256);

    for (int i = 0; i < total; i++) {
        // valid entry
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == 0xE5) continue;
        if (entries[i].attr == FAT16_ATTR_LFN) continue;

        if (k_memcmp(entries[i].name, name83, 11) == 0) {

            // compute real LBA + index
            uint32_t lba = fat16_entry_lba(cwd, i);
            int index    = i % 16;

            *outEntry = entries[i];
            *outLBA   = lba;
            *outIndex = index;
            return 1;
        }
    }

    return 0;
}
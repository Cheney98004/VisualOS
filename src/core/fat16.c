#include <stdint.h>
#include "string.h"
#include "ide.h"
#include "terminal.h"

#define BPS 512
#define ROOT_START 19
#define ROOT_SECTORS 14
#define ROOT_DIR_SIZE (ROOT_SECTORS * BPS)

static uint8_t root_dir[ROOT_DIR_SIZE];

static void fat16_load_root() {
    for (int i = 0; i < ROOT_SECTORS; i++) {
        ide_read_sector(ROOT_START + i, root_dir + i * BPS);
    }
}

static void fat16_write_root() {
    for (int i = 0; i < ROOT_SECTORS; i++) {
        ide_write_sector(ROOT_START + i, root_dir + i * BPS);
    }
}

static void fat16_format_name(char *out, const char *name) {
    k_memset(out, ' ', 11);

    int oi = 0;

    // NAME (8 chars)
    for (int i = 0; name[i] && name[i] != '.'; i++) {
        if (oi >= 8) break;

        char c = name[i];

        // inline uppercase
        if (c >= 'a' && c <= 'z')
            c = c - 32;

        out[oi++] = c;
    }

    // find dot manually (no strchr)
    const char *ext = NULL;
    for (int i = 0; name[i]; i++) {
        if (name[i] == '.') {
            ext = &name[i + 1];
            break;
        }
    }

    // EXT (3 chars)
    if (ext) {
        for (int i = 0; ext[i] && i < 3; i++) {
            char c = ext[i];
            if (c >= 'a' && c <= 'z')
                c = c - 32;
            out[8 + i] = c;
        }
    }
}

int fat16_create_file(const char *name) {
    fat16_load_root();

    // search empty entry (0x00 or 0xE5)
    for (int i = 0; i < ROOT_DIR_SIZE; i += 32) {
        uint8_t *e = &root_dir[i];

        if (e[0] == 0x00 || e[0] == 0xE5) {
            char fname[11];
            fat16_format_name(fname, name);
            k_memcpy(e + 0,  fname, 11);

            e[11] = 0x20;          // archive attribute
            e[12] = 0;             // reserved
            e[22] = 0; e[23] = 0;  // time
            e[24] = 0; e[25] = 0;  // date
            e[26] = 0; e[27] = 0;  // start cluster = 0
            e[28] = 0; e[29] = 0; e[30] = 0; e[31] = 0; // size = 0

            fat16_write_root();
            terminal_write("FAT16: created file ");
            terminal_write(name);
            terminal_write("\n");

            return 1;
        }
    }

    terminal_write_line("FAT16: root directory full");
    return 0;
}

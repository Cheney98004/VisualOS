#include "ide.h"
#include "fs.h"
#include "terminal.h"
#include <stdint.h>
#include "string.h"

#define ROOT_SECTOR 19
#define ROOT_ENTRIES 224
#define ROOT_SECTORS 14

typedef struct {
    char     name[11]; 
    uint8_t  attr;
    uint8_t  ntReserved;
    uint8_t  creationTenths;
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t accessDate;
    uint16_t highCluster;   // FAT16 = 0
    uint16_t modTime;
    uint16_t modDate;
    uint16_t cluster;
    uint32_t size;
} __attribute__((packed)) DirEntry;

void fat16_load_root()
{
    uint8_t sector[512];

    for (int i = 0; i < ROOT_SECTORS; i++) {
        ide_read_sector(ROOT_SECTOR + i, sector);

        DirEntry *e = (DirEntry*)sector;

        for (int j = 0; j < 512 / 32; j++, e++) {

            // End of directory
            if (e->name[0] == 0x00)
                return;

            // Deleted
            if (e->name[0] == 0xE5)
                continue;

            // Long filename entry
            if (e->attr == 0x0F)
                continue;

            // Convert FAT16 8.3 to normal name
            char fname[16];
            int p = 0;

            // Name part
            for (int k = 0; k < 8; k++) {
                if (e->name[k] == ' ')
                    break;
                fname[p++] = e->name[k];
            }

            // Extension
            if (e->name[8] != ' ') {
                fname[p++] = '.';
                for (int k = 8; k < 11; k++) {
                    if (e->name[k] == ' ')
                        break;
                    fname[p++] = e->name[k];
                }
            }

            fname[p] = 0;

            // Add to RAM FS (ignore kernel.bin)
            if (k_strcmp(fname, "KERNEL.BIN") != 0)
                fs_touch((FsNode*)fs_root(), fname);
        }
    }
}

#include "fat16.h"
#include "ide.h"
#include "string.h"
#include "terminal.h"

/* ============================================================================
   Global FAT16 State
   ============================================================================
*/
Fat16BPB bpb;

uint32_t fatStart;
uint32_t rootStart;
uint32_t dataStart;

uint32_t rootEntryCount;
uint32_t rootDirSectors;

static uint8_t fatBuffer[FAT16_SECTOR_SIZE * 64];
static uint8_t rootBuffer[FAT16_SECTOR_SIZE * 32];   // up to 16KB root

/* ============================================================================
   Sector I/O
   ============================================================================
*/
void fat16_read_sector(uint32_t lba, void *buf) {
    ide_read_sector(lba, buf);
}

void fat16_write_sector(uint32_t lba, const void *buf) {
    ide_write_sector(lba, buf);
}

/* ============================================================================
   FAT read / write
   ============================================================================
*/
uint16_t fat16_get_fat(uint16_t cluster) {
    uint32_t offset = cluster * 2;
    uint32_t sector = offset / 512;
    uint32_t idx    = offset % 512;

    return *(uint16_t *)(fatBuffer + sector * 512 + idx);
}

void fat16_set_fat(uint16_t cluster, uint16_t value) {
    uint32_t offset = cluster * 2;
    uint32_t sector = offset / 512;
    uint32_t idx    = offset % 512;

    *(uint16_t *)(fatBuffer + sector * 512 + idx) = value;
}

void fat16_flush_fat() {
    for (uint32_t i = 0; i < bpb.fatSize; i++)
        fat16_write_sector(fatStart + i, fatBuffer + i * 512);
}

void fat16_flush_root() {
    for (uint32_t i = 0; i < rootDirSectors; i++)
        fat16_write_sector(rootStart + i, rootBuffer + i * 512);
}

/* ============================================================================
   Format 8.3 name
   ============================================================================
*/
void fat16_format_83(char out[11], const char *name) {
    k_memset(out, ' ', 11);

    int p = 0;

    // name
    while (*name && *name != '.' && p < 8) {
        char c = *name++;
        if (c >= 'a' && c <= 'z') c -= 32;
        out[p++] = c;
    }

    // extension
    const char *ext = k_strchr(name, '.');
    if (ext) {
        ext++;
        for (int i = 0; i < 3 && ext[i]; i++) {
            char c = ext[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[8 + i] = c;
        }
    }
}

/* ============================================================================
   Init
   ============================================================================
*/
int fat16_init() {
    uint8_t boot[512];
    fat16_read_sector(0, boot);

    k_memcpy(&bpb, boot, sizeof(Fat16BPB));

    if (bpb.bytesPerSector != 512)
        return 0;

    rootEntryCount  = bpb.rootEntryCount;
    rootDirSectors  = (rootEntryCount * 32 + 511) / 512;

    fatStart  = bpb.reservedSectors;
    rootStart = fatStart + bpb.fatCount * bpb.fatSize;
    dataStart = rootStart + rootDirSectors;

    // Read FAT
    for (uint32_t i = 0; i < bpb.fatSize; i++)
        fat16_read_sector(fatStart + i, fatBuffer + i * 512);

    // Read ROOT
    for (uint32_t i = 0; i < rootDirSectors; i++)
        fat16_read_sector(rootStart + i, rootBuffer + i * 512);

    terminal_write_line("FAT16 OK");
    return 1;
}

/* ============================================================================
   Directory scanning
   ============================================================================*/
int fat16_find_in_dir(uint32_t dirCluster, const char *name83) {
    (void)dirCluster; // only root supported now

    Fat16DirEntry *ent = (Fat16DirEntry *)rootBuffer;

    for (uint32_t i = 0; i < rootEntryCount; i++) {
        if (ent[i].name[0] == 0x00)
            return -1;
        if (ent[i].name[0] == 0xE5)
            continue;
        if (k_memcmp(ent[i].name, name83, 11) == 0)
            return i;
    }
    return -1;
}

/* ============================================================================
   List directory
   ============================================================================
*/
int fat16_list_dir(uint32_t dirCluster, char names[][13], int max) {
    (void)dirCluster;

    Fat16DirEntry *ent = (Fat16DirEntry *)rootBuffer;
    int n = 0;

    for (uint32_t i = 0; i < rootEntryCount && n < max; i++) {
        if (ent[i].name[0] == 0x00)
            break;
        if (ent[i].name[0] == 0xE5)
            continue;
        if (ent[i].attr == FAT16_ATTR_LFN)
            continue;

        char *out = names[n];
        int p = 0;

        for (int j = 0; j < 8; j++)
            if (ent[i].name[j] != ' ')
                out[p++] = ent[i].name[j];

        if (ent[i].name[8] != ' ') {
            out[p++] = '.';
            for (int j = 8; j < 11; j++)
                if (ent[i].name[j] != ' ')
                    out[p++] = ent[i].name[j];
        }

        out[p] = 0;
        n++;
    }

    return n;
}

/* ============================================================================
   Allocate cluster
   ============================================================================
*/
uint16_t fat16_alloc_cluster() {
    for (uint16_t c = 2; c < 4096; c++) {
        if (fat16_get_fat(c) == 0x0000) {
            fat16_set_fat(c, 0xFFFF);
            return c;
        }
    }
    return 0;
}

/* ============================================================================
   Create file
   ============================================================================
*/
int fat16_create_file(const char *filename) {
    char name83[11];
    fat16_format_83(name83, filename);

    if (fat16_find_in_dir(0, name83) >= 0)
        return 0;

    Fat16DirEntry *ent = (Fat16DirEntry *)rootBuffer;

    for (uint32_t i = 0; i < rootEntryCount; i++) {
        if (ent[i].name[0] == 0x00 || ent[i].name[0] == 0xE5) {

            k_memcpy(ent[i].name, name83, 11);
            ent[i].attr = FAT16_ATTR_ARCHIVE;
            ent[i].clusterLow = 0;
            ent[i].fileSize = 0;

            fat16_flush_root();
            return 1;
        }
    }
    return 0;
}

/* ============================================================================
   Read file
   ============================================================================
*/
uint32_t fat16_read_file(const char *name, void *buffer, uint32_t maxSize) {
    char name83[11];
    fat16_format_83(name83, name);

    int idx = fat16_find_in_dir(0, name83);
    if (idx < 0) return 0;

    Fat16DirEntry *e = (Fat16DirEntry *)rootBuffer + idx;

    uint16_t cl = e->clusterLow;
    uint8_t *out = buffer;
    uint32_t left = e->fileSize;

    while (cl >= 2 && cl < 0xFFF8 && left > 0) {
        uint8_t sec[512];
        fat16_read_sector(dataStart + (cl - 2), sec);

        uint32_t n = (left > 512 ? 512 : left);
        if (n > maxSize) n = maxSize;

        k_memcpy(out, sec, n);
        out += n;
        left -= n;
        maxSize -= n;

        cl = fat16_get_fat(cl);
    }

    return e->fileSize;
}

/* ============================================================================
   Write file (overwrite)
   ============================================================================
*/
int fat16_write_file(const char *name, const void *buffer, uint32_t size) {
    char name83[11];
    fat16_format_83(name83, name);

    int idx = fat16_find_in_dir(0, name83);
    if (idx < 0) return 0;

    Fat16DirEntry *e = (Fat16DirEntry *)rootBuffer + idx;

    // free old chain
    uint16_t cl = e->clusterLow;
    while (cl >= 2 && cl < 0xFFF8) {
        uint16_t next = fat16_get_fat(cl);
        fat16_set_fat(cl, 0x0000);
        cl = next;
    }

    // allocate new
    uint16_t prev = 0;
    uint32_t written = 0;

    e->clusterLow = 0;

    while (written < size) {
        uint16_t newCl = fat16_alloc_cluster();
        if (!newCl) return 0;

        if (e->clusterLow == 0)
            e->clusterLow = newCl;

        if (prev)
            fat16_set_fat(prev, newCl);

        uint8_t sec[512];
        k_memset(sec, 0, 512);

        uint32_t n = (size - written > 512 ? 512 : size - written);

        k_memcpy(sec, (uint8_t*)buffer + written, n);
        fat16_write_sector(dataStart + (newCl - 2), sec);

        written += n;
        prev = newCl;
    }

    fat16_set_fat(prev, 0xFFFF);

    e->fileSize = size;

    fat16_flush_fat();
    fat16_flush_root();

    return 1;
}

/* ============================================================================
   Delete file
   ============================================================================
*/
int fat16_delete(const char *filename) {
    char name83[11];
    fat16_format_83(name83, filename);

    int idx = fat16_find_in_dir(0, name83);
    if (idx < 0) return 0;

    Fat16DirEntry *e = (Fat16DirEntry *)rootBuffer + idx;

    // free clusters
    uint16_t cl = e->clusterLow;
    while (cl >= 2 && cl < 0xFFF8) {
        uint16_t next = fat16_get_fat(cl);
        fat16_set_fat(cl, 0x0000);
        cl = next;
    }

    e->name[0] = 0xE5;

    fat16_flush_root();
    fat16_flush_fat();
    return 1;
}

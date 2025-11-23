#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

#define FAT16_SECTOR_SIZE    512
#define FAT16_MAX_FILENAME   12     // "XXXXXXXX.XXX\0"

// FAT attributes
#define FAT16_ATTR_READONLY  0x01
#define FAT16_ATTR_HIDDEN    0x02
#define FAT16_ATTR_SYSTEM    0x04
#define FAT16_ATTR_VOLUMEID  0x08
#define FAT16_ATTR_DIRECTORY 0x10
#define FAT16_ATTR_ARCHIVE   0x20
#define FAT16_ATTR_LFN       0x0F   // long filename entry

// End-of-chain marker
#define FAT16_EOC            0xFFFF

// ---- Permission Flags ----
// 可讀
#define PERM_R 0x01
// 可寫
#define PERM_W 0x02
// 可執行
#define PERM_X 0x04
// 隱藏
#define PERM_H 0x08
// 系統檔案（不可刪除）
#define PERM_S 0x10
// immutable（不可修改、不可刪除、不可寫）
#define PERM_I 0x20

// FAT16 BIOS Parameter Block (BPB)
typedef struct {
    uint16_t bytesPerSector;      // 512
    uint8_t  sectorsPerCluster;   // typically 1
    uint16_t reservedSectors;     // Boot sector count
    uint8_t  fatCount;            // usually 2
    uint16_t rootEntryCount;      // 224 for FAT12, 512 for FAT16
    uint16_t totalSectors16;
    uint8_t  mediaType;
    uint16_t fatSize16;           // FAT size (in sectors)
    uint16_t sectorsPerTrack;
    uint16_t numHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;
} __attribute__((packed)) Fat16BPB;

// FAT16 directory entry (32 bytes)
typedef struct {
    char     name[11];
    uint8_t  attr;
    uint8_t  flags;     // <--- Original ntReserved
    uint8_t  uid;       // <--- Original creationTenths
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t accessDate;
    uint16_t clusterHigh;
    uint16_t modTime;
    uint16_t modDate;
    uint16_t cluster;
    uint32_t size;
} __attribute__((packed)) Fat16DirEntry;

// ============================================================
// Public API
// ============================================================

int fat16_init();

// directory handling
void fat16_list_directory(uint16_t dirCluster);
int  fat16_find_in_directory(uint16_t dirCluster, const char *name);
int  fat16_load_directory(uint16_t dirCluster, Fat16DirEntry *out, int max);
int  fat16_list_dir(uint16_t dirCluster, char names[][13], int max);
int  fat16_find_in_dir(uint16_t dirCluster, const char *name);

// file operations
int      fat16_write_file(const char *filename, const void *data, uint32_t size);
uint32_t fat16_read_file(const char *filename, void *buffer, uint32_t maxSize);
int      fat16_create_file(const char *filename);
int      fat16_delete(const char *filename);

// directory create / delete
int fat16_mkdir(const char *name);
int fat16_delete_file(const char *name);
int fat16_create_directory(const char *name);

// change directory
int      fat16_cd(const char *path);
uint16_t fat16_get_cwd();
void     fat16_set_cwd(uint16_t cl);

// name helpers
void fat16_format_83(char out[11], const char *name);
void fat16_decode_name(char out[13], const char *name83);

void fat16_get_path(char *out);
int fat16_find_name_by_cluster(uint16_t parentCl, uint16_t targetCl, char out[13]);

int fat16_get_entry(uint32_t lba, int index, Fat16DirEntry *out);
int fat16_set_entry(uint32_t lba, int index, const Fat16DirEntry *ent);

void fat16_protect_kernel();

uint32_t fat16_entry_lba(uint16_t dirCluster, int entryIndex);

int fat16_rename(const char *oldName, const char *newName);

#endif

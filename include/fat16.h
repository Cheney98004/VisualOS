#ifndef VISUALOS_FAT16_H
#define VISUALOS_FAT16_H

#include <stdint.h>

/* ============================================================================
   FAT16 BIOS Parameter Block (BPB)
   Read directly from LBA 0 (boot sector)
   ============================================================================
*/
#pragma pack(push, 1)
typedef struct {
    uint8_t     jmp[3];
    char        oem[8];
    uint16_t    bytesPerSector;     // Usually 512
    uint8_t     sectorsPerCluster;  // Usually 1 for 1.44MB
    uint16_t    reservedSectors;    // Usually 1 (boot sector)
    uint8_t     fatCount;           // Usually 2
    uint16_t    rootEntryCount;     // Usually 224
    uint16_t    totalSectors16;     // 2880 for 1.44MB
    uint8_t     mediaDescriptor;    // F0 for floppy
    uint16_t    fatSize;            // FAT size in sectors (usually 9)
    uint16_t    sectorsPerTrack;
    uint16_t    headCount;
    uint32_t    hiddenSectors;
    uint32_t    totalSectors32;

    uint8_t     driveNumber;
    uint8_t     reserved;
    uint8_t     bootSignature;
    uint32_t    volumeID;
    char        volumeLabel[11];
    char        fileSystemType[8];
} Fat16BPB;
#pragma pack(pop)

/* ============================================================================
   Directory Entry (32 bytes)
   ============================================================================
*/
#pragma pack(push, 1)
typedef struct {
    char        name[11];       // 8.3 name
    uint8_t     attr;           // Attributes
    uint8_t     ntReserved;
    uint8_t     creationTenths;
    uint16_t    creationTime;
    uint16_t    creationDate;
    uint16_t    lastAccessDate;
    uint16_t    clusterHigh;    // Always 0 for FAT16
    uint16_t    writeTime;
    uint16_t    writeDate;
    uint16_t    clusterLow;     // First cluster
    uint32_t    fileSize;       // File size in bytes
} Fat16DirEntry;
#pragma pack(pop)

/* ============================================================================
   FAT16 constants
   ============================================================================
*/
#define FAT16_SECTOR_SIZE 512
#define FAT16_MAX_NAME    11

/* FAT attribute bits */
#define FAT16_ATTR_READONLY  0x01
#define FAT16_ATTR_HIDDEN    0x02
#define FAT16_ATTR_SYSTEM    0x04
#define FAT16_ATTR_VOLUMEID  0x08
#define FAT16_ATTR_DIRECTORY 0x10
#define FAT16_ATTR_ARCHIVE   0x20
#define FAT16_ATTR_LFN       0x0F   // Long File Name entry

/* ============================================================================
   Global state (initialized by fat16_init)
   ============================================================================
*/
extern Fat16BPB bpb;  
extern uint32_t fatStart;     
extern uint32_t rootStart;    
extern uint32_t dataStart;    
extern uint32_t rootEntryCount;
extern uint32_t rootDirSectors;

/* ============================================================================
   Core FAT16 API
   ============================================================================
*/

/* Initialize FAT16 (read BPB, FAT, Root info) */
int fat16_init();

/* List a directory (root or subdirectory) */
int fat16_list_dir(uint32_t dirCluster, char names[][13], int max);

/* Find directory entry inside a directory */
int fat16_find_in_dir(uint32_t dirCluster, const char *name83);

/* Create a file in root directory (temp restriction) */
int fat16_create_file(const char *filename);

/* Create directory */
int fat16_mkdir(const char *name);

/* Delete file */
int fat16_delete(const char *filename);

/* Read a file fully into buffer */
uint32_t fat16_read_file(const char *name, void *buffer, uint32_t maxSize);

/* Write to a file (overwrite) */
int fat16_write_file(const char *name, const void *buffer, uint32_t size);

/* ============================================================================
   Helpers
   ============================================================================
*/

/* Convert normal string → 8.3 name */
void fat16_format_83(char out[11], const char *name);

/* Cluster allocation helpers */
uint16_t fat16_alloc_cluster();
uint16_t fat16_get_fat(uint16_t cluster);
void     fat16_set_fat(uint16_t cluster, uint16_t value);

/* Sync FAT to disk */
void fat16_flush_fat();

/* Sync root directory */
void fat16_flush_root();

/* Read/write sector helpers */
void fat16_read_sector(uint32_t lba, void *buf);
void fat16_write_sector(uint32_t lba, const void *buf);

#endif

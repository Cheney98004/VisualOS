#include "fat16.h"
#include "ide.h"
#include "terminal.h"
#include "string.h"

#define FAT16_EOC      0xFFFF
#define FAT16_FREE     0x0000

static Fat16BPB bpb;                 // boot sector ( BPB )
static uint16_t fatTable[65536];     // supports up to 65536 clusters

static uint32_t rootDirStartLBA;
static uint32_t dataStartLBA;

// ============================================================
//  Read sector helper
// ============================================================
static inline void read_sector(uint32_t lba, void *buf) {
    ide_read_sector(lba, buf);
}

static inline void write_sector(uint32_t lba, const void *buf) {
    ide_write_sector(lba, buf);
}

// ============================================================
//  FAT16 Initialization
// ============================================================
int fat16_init() {
    uint8_t sector[FAT16_SECTOR_SIZE];

    // Load boot sector
    read_sector(0, sector);
    k_memcpy(&bpb, sector + 11, sizeof(Fat16BPB));

    if (bpb.bytesPerSector != 512) {
        terminal_write_line("[FAT16] Unsupported sector size");
        return 0;
    }

    // Calculate FAT and data area positions
    uint32_t rootDirSectors =
        ((bpb.rootEntryCount * 32) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector;

    rootDirStartLBA = bpb.reservedSectors + (bpb.fatSize16 * bpb.fatCount);
    dataStartLBA    = rootDirStartLBA + rootDirSectors;

    // Load FAT table (only first FAT)
    for (uint32_t i = 0; i < bpb.fatSize16; i++) {
        uint8_t sec[FAT16_SECTOR_SIZE];
        read_sector(bpb.reservedSectors + i, sec);
        k_memcpy(((uint8_t *)fatTable) + i * FAT16_SECTOR_SIZE, sec, FAT16_SECTOR_SIZE);
    }

    terminal_write_line("FAT16 initialized.");
    return 1;
}

// ============================================================
// FAT table helpers
// ============================================================
static inline uint16_t fat_get(uint16_t cluster) {
    return fatTable[cluster];
}

static inline void fat_set(uint16_t cluster, uint16_t value) {
    fatTable[cluster] = value;
}

// Write FAT back to disk
static void fat_flush() {
    uint8_t buf[FAT16_SECTOR_SIZE];

    for (uint32_t i = 0; i < bpb.fatSize16; i++) {
        k_memcpy(buf, ((uint8_t *)fatTable) + i * FAT16_SECTOR_SIZE, FAT16_SECTOR_SIZE);

        // write to both FATs
        write_sector(bpb.reservedSectors + i, buf);
        write_sector(bpb.reservedSectors + i + bpb.fatSize16, buf);
    }
}

// ============================================================
// Allocate a free cluster
// ============================================================
static uint16_t fat_alloc_cluster() {
    for (uint16_t c = 2; c < 65535; c++) {
        if (fat_get(c) == FAT16_FREE) {
            fat_set(c, FAT16_EOC);
            fat_flush();
            return c;
        }
    }
    return 0;   // disk full
}

// ============================================================
// Cluster → LBA
// ============================================================
static inline uint32_t cluster_to_lba(uint16_t cl) {
    return dataStartLBA + (cl - 2) * bpb.sectorsPerCluster;
}

// ============================================================
// Clear cluster to zeros
// ============================================================
static void clear_cluster(uint16_t cl) {
    uint8_t zero[FAT16_SECTOR_SIZE];
    k_memset(zero, 0, FAT16_SECTOR_SIZE);

    uint32_t lba = cluster_to_lba(cl);
    for (uint8_t i = 0; i < bpb.sectorsPerCluster; i++) {
        write_sector(lba + i, zero);
    }
}

// ============================================================
// Cluster chain traversal
// ============================================================
static uint16_t fat_next(uint16_t cl) {
    uint16_t n = fat_get(cl);
    if (n >= 0xFFF8) return 0; // EOC
    return n;
}

// ============================================================
// Allocate new cluster and link chain
// ============================================================
static uint16_t fat_extend_chain(uint16_t lastCl) {
    uint16_t newCl = fat_alloc_cluster();
    if (!newCl) return 0;

    fat_set(lastCl, newCl);
    fat_flush();
    clear_cluster(newCl);

    return newCl;
}

// ============================================================
// Read an entire cluster into buffer
// ============================================================
static void read_cluster(uint16_t cl, void *buf) {
    uint32_t lba = cluster_to_lba(cl);
    for (uint8_t i = 0; i < bpb.sectorsPerCluster; i++)
        read_sector(lba + i, ((uint8_t *)buf) + i * FAT16_SECTOR_SIZE);
}

// ============================================================
// Write an entire cluster
// ============================================================
static void write_cluster(uint16_t cl, const void *buf) {
    uint32_t lba = cluster_to_lba(cl);
    for (uint8_t i = 0; i < bpb.sectorsPerCluster; i++)
        write_sector(lba + i, ((uint8_t *)buf) + i * FAT16_SECTOR_SIZE);
}

// Load a single directory entry block (512 bytes)
static void load_dir_sector(uint32_t lba, Fat16DirEntry *entries) {
    uint8_t buf[FAT16_SECTOR_SIZE];
    read_sector(lba, buf);
    k_memcpy(entries, buf, FAT16_SECTOR_SIZE);
}

// Save directory block back to disk
static void save_dir_sector(uint32_t lba, const Fat16DirEntry *entries) {
    uint8_t buf[FAT16_SECTOR_SIZE];
    k_memcpy(buf, entries, FAT16_SECTOR_SIZE);
    write_sector(lba, buf);
}

// ------------------------------------------------------------
// Load entire directory (root or subdirectory)
// ------------------------------------------------------------
// dirCluster == 0  → root directory
// dirCluster >= 2  → subdirectory in clusters
//
// Returns number of entries written into "out" buffer.
// "max" = maximum entries to store.
//
int fat16_load_directory(uint16_t dirCluster, Fat16DirEntry *out, int max) {
    int count = 0;

    if (dirCluster == 0) {
        // root directory is fixed area, not cluster based
        uint32_t rootEntries = bpb.rootEntryCount;
        uint32_t sectors = ((rootEntries * 32) + 511) / 512;

        for (uint32_t i = 0; i < sectors; i++) {
            Fat16DirEntry block[16];
            uint32_t lba = rootDirStartLBA + i;

            load_dir_sector(lba, block);

            for (int j = 0; j < 16; j++) {
                uint8_t first = block[j].name[0];

                if (first == 0x00) {
                    return count;
                }
                // deleted
                if (first == 0xE5)
                    continue;

                // LFN
                if (block[j].attr == FAT16_ATTR_LFN)
                    continue;

                // valid
                if (count < max)
                    out[count++] = block[j];
            }
        }
        return count;
    }

    // -----------------------------
    // Subdirectory (cluster chain)
    // -----------------------------
    uint16_t cl = dirCluster;

    while (cl >= 2 && count < max) {
        Fat16DirEntry block[16];

        uint32_t lba = cluster_to_lba(cl);

        for (uint8_t s = 0; s < bpb.sectorsPerCluster; s++) {
            load_dir_sector(lba + s, block);

            for (int j = 0; j < 16; j++) {

                uint8_t first = block[j].name[0];

                if (first == 0x00) {
                    return count;   // STOP EVERYTHING
                }

                if (first == 0xE5) continue;
                if (block[j].attr == FAT16_ATTR_LFN) continue;

                if (count < max)
                    out[count++] = block[j];
            }
        }

        // next cluster
        cl = fat_next(cl);
    }

    return count;
}

// ------------------------------------------------------------
// Search for file/dir in directory
// Returns index in directory or -1 if not found
// ------------------------------------------------------------
int fat16_find_in_directory(uint16_t dirCluster, const char *name) {
    Fat16DirEntry entries[256];
    int n = fat16_load_directory(dirCluster, entries, 256);

    char search83[11];
    fat16_format_83(search83, name);

    for (int i = 0; i < n; i++) {
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == 0xE5) continue;
        if (entries[i].attr == FAT16_ATTR_LFN) continue;

        if (!k_memcmp(entries[i].name, search83, 11)) {
            return i;
        }
    }
    return -1;
}

// ------------------------------------------------------------
// List directory contents
// ------------------------------------------------------------
void fat16_list_directory(uint16_t dirCluster) {
    Fat16DirEntry entries[256];
    int n = fat16_load_directory(dirCluster, entries, 256);

    char temp[13];

    for (int i = 0; i < n; i++) {
        Fat16DirEntry *e = &entries[i];

        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5) continue;
        if (e->attr == FAT16_ATTR_LFN) continue;

        // Convert 8.3 name to regular string
        fat16_decode_name(temp, e->name);

        terminal_write_line(temp);
    }
}

// ------------------------------------------------------------
// Allocate a free directory entry slot
// Returns sector LBA + index
// ------------------------------------------------------------
static int fat16_find_free_entry(uint16_t dirCluster, uint32_t *outLBA, int *outIndex) {
    Fat16DirEntry block[16];

    if (dirCluster == 0) {
        // root directory
        uint32_t rootEntries = bpb.rootEntryCount;
        uint32_t sectors = ((rootEntries * 32) + 511) / 512;

        for (uint32_t i = 0; i < sectors; i++) {
            uint32_t lba = rootDirStartLBA + i;
            load_dir_sector(lba, block);

            for (int j = 0; j < 16; j++) {
                if (block[j].name[0] == 0x00 || block[j].name[0] == 0xE5) {
                    *outLBA = lba;
                    *outIndex = j;
                    return 1;
                }
            }
        }
        return 0;
    }

    // Subdirectory
    uint16_t cl = dirCluster;
    while (cl >= 2) {
        uint32_t lba = cluster_to_lba(cl);

        for (uint8_t i = 0; i < bpb.sectorsPerCluster; i++) {
            load_dir_sector(lba + i, block);

            for (int j = 0; j < 16; j++) {
                if (block[j].name[0] == 0x00 || block[j].name[0] == 0xE5) {
                    *outLBA = lba + i;
                    *outIndex = j;
                    return 1;
                }
            }
        }
        cl = fat_next(cl);
    }

    return 0; // no space
}

// ------------------------------------------------------------
// Create directory entry — core helper (file or directory)
// ------------------------------------------------------------
int fat16_write_entry(uint16_t dirCluster, const Fat16DirEntry *newEntry) {
    uint32_t lba;
    int index;

    if (!fat16_find_free_entry(dirCluster, &lba, &index))
        return 0;

    Fat16DirEntry block[16];
    load_dir_sector(lba, block);

    block[index] = *newEntry;

    save_dir_sector(lba, block);
    return 1;
}

// current working directory cluster
// 0 = root
uint16_t currentDirCluster = 0;

// external accessor for shell
uint16_t fat16_get_cwd() {
    return currentDirCluster;
}

void fat16_set_cwd(uint16_t cl) {
    currentDirCluster = cl;
}

// ------------------------------------------------------------
// Initialize a directory cluster: '.', '..'
// ------------------------------------------------------------
static void fat16_init_directory_cluster(uint16_t newCl, uint16_t parentCl) {
    // clear the entire cluster before writing '.' and '..'
    clear_cluster(newCl);

    // prepare array of entries
    Fat16DirEntry block[16];
    k_memset(block, 0, sizeof(block));

    // "." entry
    k_memset(block[0].name, ' ', 11);
    block[0].name[0] = '.';
    block[0].attr = FAT16_ATTR_DIRECTORY;
    block[0].cluster = newCl;
    block[0].size = 0;

    // ".." entry
    k_memset(block[1].name, ' ', 11);
    block[1].name[0] = '.';
    block[1].name[1] = '.';
    block[1].attr = FAT16_ATTR_DIRECTORY;
    block[1].cluster = parentCl;
    block[1].size = 0;

    // write first 32 entries (fits into 1 sector)
    uint32_t lba = cluster_to_lba(newCl);
    save_dir_sector(lba, block);
}

// ------------------------------------------------------------
// mkdir implementation
// ------------------------------------------------------------
int fat16_mkdir(const char *name) {
    char name83[11];
    fat16_format_83(name83, name);

    // check name conflict
    if (fat16_find_in_directory(currentDirCluster, name) >= 0)
        return 0;

    // allocate cluster
    uint16_t newCl = fat_alloc_cluster();
    if (newCl == 0) return 0;

    // create "." and ".."
    fat16_init_directory_cluster(newCl, currentDirCluster);

    // build directory entry
    Fat16DirEntry e;
    k_memset(&e, 0, sizeof(e));
    k_memcpy(e.name, name83, 11);
    e.attr = FAT16_ATTR_DIRECTORY;
    e.cluster = newCl;
    e.size = 0;

    // write into current directory
    return fat16_write_entry(currentDirCluster, &e);
}

// ------------------------------------------------------------
// Change directory
// ------------------------------------------------------------
int fat16_cd(const char *path) {
    // absolute -> go to root
    if (path[0] == '/') {
        // jump to root first
        currentDirCluster = 0;

        // skip leading '/'
        path++;

        // empty path -> just "/"
        if (*path == 0)
            return 1;

        // recursively cd into next component
        return fat16_cd(path);
    }

    // ".."
    if (!k_strcmp(path, "..")) {
        if (currentDirCluster == 0) return 1; // already root

        // load current dir to get ".."
        Fat16DirEntry entries[256];
        int n = fat16_load_directory(currentDirCluster, entries, 256);

        for (int i = 0; i < n; i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5) continue;

            if (entries[i].attr == FAT16_ATTR_DIRECTORY &&
                entries[i].name[0] == '.' &&
                entries[i].name[1] == '.') {
                currentDirCluster = entries[i].cluster;
                return 1;
            }
        }
        return 0;
    }

    // normal subdirectory
    int idx = fat16_find_in_directory(currentDirCluster, path);
    if (idx < 0) return 0;

    Fat16DirEntry entries[256];
    fat16_load_directory(currentDirCluster, entries, 256);

    if (!(entries[idx].attr & FAT16_ATTR_DIRECTORY))
        return 0; // not a directory

    currentDirCluster = entries[idx].cluster;
    return 1;
}

// ------------------------------------------------------------
// Name helpers
// ------------------------------------------------------------
void fat16_format_83(char out[11], const char *name) {
    // Initialize with spaces
    for (int i = 0; i < 11; i++) out[i] = ' ';

    int i = 0;
    int j = 0;
    // name part (up to 8)
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        out[j++] = c;
        i++;
    }

    // skip dot if present
    if (name[i] == '.') i++;

    // extension (up to 3)
    j = 8;
    int k = 0;
    while (name[i] && k < 3) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        out[j++] = c;
        i++;
        k++;
    }
}

void fat16_decode_name(char out[13], const char name83[11]) {
    int p = 0;

    // filename
    for (int i = 0; i < 8 && name83[i] != ' '; i++) {
        out[p++] = name83[i];
    }

    // extension
    if (name83[8] != ' ') {
        out[p++] = '.';
        for (int i = 8; i < 11 && name83[i] != ' '; i++) {
            out[p++] = name83[i];
        }
    }

    out[p] = 0;
}

// ------------------------------------------------------------
// Directory helpers
// ------------------------------------------------------------
static int fat16_find_entry(uint16_t dirCluster, const char name83[11],
                            uint32_t *outLBA, int *outIndex, Fat16DirEntry *outEntry) {
    Fat16DirEntry block[16];

    if (dirCluster == 0) {
        uint32_t rootEntries = bpb.rootEntryCount;
        uint32_t sectors = ((rootEntries * 32) + 511) / 512;

        for (uint32_t i = 0; i < sectors; i++) {
            uint32_t lba = rootDirStartLBA + i;
            load_dir_sector(lba, block);

            for (int j = 0; j < 16; j++) {
                if (block[j].name[0] == 0x00) break;
                if (block[j].name[0] == 0xE5) continue;
                if (block[j].attr == FAT16_ATTR_LFN) continue;

                if (!k_memcmp(block[j].name, name83, 11)) {
                    if (outLBA) *outLBA = lba;
                    if (outIndex) *outIndex = j;
                    if (outEntry) *outEntry = block[j];
                    return 1;
                }
            }
        }
        return 0;
    }

    uint16_t cl = dirCluster;
    while (cl >= 2) {
        uint32_t lba = cluster_to_lba(cl);

        for (uint8_t i = 0; i < bpb.sectorsPerCluster; i++) {
            load_dir_sector(lba + i, block);

            for (int j = 0; j < 16; j++) {
                if (block[j].name[0] == 0x00) break;
                if (block[j].name[0] == 0xE5) continue;
                if (block[j].attr == FAT16_ATTR_LFN) continue;

                if (!k_memcmp(block[j].name, name83, 11)) {
                    if (outLBA) *outLBA = lba + i;
                    if (outIndex) *outIndex = j;
                    if (outEntry) *outEntry = block[j];
                    return 1;
                }
            }
        }
        cl = fat_next(cl);
    }

    return 0;
}

static void fat16_store_entry(uint32_t lba, int index, const Fat16DirEntry *entry) {
    Fat16DirEntry block[16];
    load_dir_sector(lba, block);
    block[index] = *entry;
    save_dir_sector(lba, block);
}

static void fat16_free_chain(uint16_t cl) {
    while (cl >= 2) {
        uint16_t next = fat_next(cl);
        fat_set(cl, FAT16_FREE);
        if (next == 0) break;
        cl = next;
    }
    fat_flush();
}

int fat16_list_dir(uint16_t dirCluster, char names[][13], int max) {
    for (int i = 0; i < max; i++)
        names[i][0] = '\0';

    Fat16DirEntry entries[256];
    int n = fat16_load_directory(dirCluster, entries, 256);

    int count = 0;
    for (int i = 0; i < n && count < max; i++) {
        Fat16DirEntry *e = &entries[i];

        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5) continue;
        if (e->attr == FAT16_ATTR_LFN) continue;

        fat16_decode_name(names[count], e->name);
        count++;
    }
    return count;
}

int fat16_find_in_dir(uint16_t dirCluster, const char *name) {
    char search83[11];
    fat16_format_83(search83, name);

    uint32_t lba;
    int idx;
    Fat16DirEntry e;
    if (!fat16_find_entry(dirCluster, search83, &lba, &idx, &e))
        return -1;

    return e.cluster;
}

// ------------------------------------------------------------
// File operations
// ------------------------------------------------------------
int fat16_create_file(const char *filename) {
    char name83[11];
    fat16_format_83(name83, filename);

    uint32_t lba;
    int idx;
    if (fat16_find_entry(currentDirCluster, name83, &lba, &idx, 0))
        return 0; // already exists

    Fat16DirEntry e;
    k_memset(&e, 0, sizeof(e));
    k_memcpy(e.name, name83, 11);
    e.attr = FAT16_ATTR_ARCHIVE;
    e.cluster = 0;
    e.size = 0;

    return fat16_write_entry(currentDirCluster, &e);
}

int fat16_delete(const char *filename) {
    char name83[11];
    fat16_format_83(name83, filename);

    uint32_t lba;
    int idx;
    Fat16DirEntry e;
    if (!fat16_find_entry(currentDirCluster, name83, &lba, &idx, &e))
        return 0;

    if (e.cluster >= 2) {
        fat16_free_chain(e.cluster);
    }

    e.name[0] = 0xE5;   // mark deleted
    e.size = 0;
    e.cluster = 0;
    fat16_store_entry(lba, idx, &e);

    return 1;
}

static int fat16_allocate_chain(int clustersNeeded, uint16_t *firstOut) {
    uint16_t first = 0;
    uint16_t prev = 0;

    for (int i = 0; i < clustersNeeded; i++) {
        uint16_t cl = fat_alloc_cluster();
        if (!cl) {
            if (first) fat16_free_chain(first);
            return 0;
        }

        if (!first) first = cl;
        if (prev) {
            fat_set(prev, cl);
        }
        prev = cl;
    }

    if (prev) fat_set(prev, FAT16_EOC);
    fat_flush();

    *firstOut = first;
    return 1;
}

int fat16_write_file(const char *filename, const void *data, uint32_t size) {
    char name83[11];
    fat16_format_83(name83, filename);

    uint32_t lba;
    int idx;
    Fat16DirEntry e;
    if (!fat16_find_entry(currentDirCluster, name83, &lba, &idx, &e))
        return 0;

    if (e.cluster >= 2) {
        fat16_free_chain(e.cluster);
        e.cluster = 0;
    }

    uint32_t clusterSize = FAT16_SECTOR_SIZE * bpb.sectorsPerCluster;
    int clustersNeeded = (size + clusterSize - 1) / clusterSize;

    if (clustersNeeded == 0) {
        e.size = 0;
        fat16_store_entry(lba, idx, &e);
        return 1;
    }

    uint16_t firstCl;
    if (!fat16_allocate_chain(clustersNeeded, &firstCl))
        return 0;

    e.cluster = firstCl;
    e.size = size;

    // write data to clusters sector-by-sector
    uint16_t cl = firstCl;
    uint32_t written = 0;
    uint8_t buf[FAT16_SECTOR_SIZE];

    while (cl && written < size) {
        uint32_t lba = cluster_to_lba(cl);

        for (uint8_t s = 0; s < bpb.sectorsPerCluster && written < size; s++) {
            uint32_t remaining = size - written;
            uint32_t toWrite = remaining < FAT16_SECTOR_SIZE ? remaining : FAT16_SECTOR_SIZE;

            if (toWrite < FAT16_SECTOR_SIZE) {
                k_memset(buf, 0, FAT16_SECTOR_SIZE);
                k_memcpy(buf, ((const uint8_t *)data) + written, toWrite);
                write_sector(lba + s, buf);
            } else {
                write_sector(lba + s, ((const uint8_t *)data) + written);
            }

            written += toWrite;
        }

        cl = fat_next(cl);
        if (cl == 0) break;
    }

    fat16_store_entry(lba, idx, &e);
    return 1;
}

uint32_t fat16_read_file(const char *filename, void *buffer, uint32_t maxSize) {
    char name83[11];
    fat16_format_83(name83, filename);

    uint32_t lba;
    int idx;
    Fat16DirEntry e;
    if (!fat16_find_entry(currentDirCluster, name83, &lba, &idx, &e))
        return 0;

    uint32_t size = e.size;
    if (size > maxSize) size = maxSize;

    uint32_t readBytes = 0;
    uint16_t cl = e.cluster;

    while (cl && readBytes < size) {
        uint32_t lba = cluster_to_lba(cl);

        for (uint8_t s = 0; s < bpb.sectorsPerCluster && readBytes < size; s++) {
            uint8_t buf[FAT16_SECTOR_SIZE];
            read_sector(lba + s, buf);

            uint32_t remaining = size - readBytes;
            uint32_t toCopy = remaining < FAT16_SECTOR_SIZE ? remaining : FAT16_SECTOR_SIZE;
            k_memcpy(((uint8_t *)buffer) + readBytes, buf, toCopy);

            readBytes += toCopy;
        }

        cl = fat_next(cl);
        if (cl == 0) break;
    }

    return readBytes;
}

// ------------------------------------------------------------
// convenience wrappers
// ------------------------------------------------------------
int fat16_create_directory(const char *name) {
    return fat16_mkdir(name);
}

int fat16_delete_file(const char *name) {
    return fat16_delete(name);
}

int fat16_find_name_by_cluster(uint16_t parentCl, uint16_t targetCl, char out[13]) {
    Fat16DirEntry entries[256];
    int n = fat16_load_directory(parentCl, entries, 256);

    for (int i = 0; i < n; i++) {
        Fat16DirEntry *e = &entries[i];
        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5) continue;
        if (e->attr == FAT16_ATTR_LFN) continue;

        if (e->cluster == targetCl) {
            fat16_decode_name(out, e->name);
            return 1;
        }
    }
    return 0;
}

void fat16_get_path(char *out) {
    char parts[16][13];
    int depth = 0;

    uint16_t cur = fat16_get_cwd();

    // root
    if (cur == 0) {
        out[0] = '/';
        out[1] = 0;
        return;
    }

    while (cur != 0) {

        // load ".."
        Fat16DirEntry entries[256];
        int n = fat16_load_directory(cur, entries, 256);

        uint16_t parent = 0;

        for (int i = 0; i < n; i++) {
            if (entries[i].name[0] == 0x00) break;

            // find ".."
            if (entries[i].name[0] == '.' &&
                entries[i].name[1] == '.') {
                parent = entries[i].cluster;
                break;
            }
        }

        // get this directory name from parent
        fat16_find_name_by_cluster(parent, cur, parts[depth]);
        depth++;

        cur = parent;
    }

    // construct path manually
    int pos = 0;
    out[pos++] = '/';

    for (int i = depth - 1; i >= 0; i--) {
        char *p = parts[i];
        while (*p) out[pos++] = *p++;
        if (i > 0) out[pos++] = '/';
    }

    out[pos] = 0;
}
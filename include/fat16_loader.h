#ifndef FAT16_LOADER_H
#define FAT16_LOADER_H

#include <stdint.h>

/**
 * 掃描 FAT16 Root Directory（sector 19–32，共 14 sectors）
 * 將其中所有短檔名 8.3 檔案載入到 RAM FS（使用 fs_touch）
 */
void fat16_load_root(void);

#endif // FAT16_LOADER_H
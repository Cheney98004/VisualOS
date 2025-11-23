#ifndef FS_H
#define FS_H

#include <stdint.h>
#include "fat16.h"

void fs_init();
void fs_list(int printHideFiles);
void fs_list_long(int printHideFiles);
int  fs_exists(const char *name);
int  fs_delete(const char *name);
int  fs_create(const char *name);
int  fs_read(const char *name, void *buf, int maxSize);
int  fs_write(const char *name, const void *buf, int size);
int  fs_mkdir(const char *name);

int fs_cd(const char *dirname);
uint16_t fs_current_dir_cluster();

int fs_get_entry(const char *name, Fat16DirEntry *outEntry, uint32_t *outLBA, int *outIndex);

#endif

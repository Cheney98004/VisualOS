#ifndef FS_H
#define FS_H

void fs_init();
void fs_list();
int  fs_exists(const char *name);
int  fs_delete(const char *name);
int  fs_create(const char *name);
int  fs_read(const char *name, void *buf, int maxSize);
int  fs_write(const char *name, const void *buf, int size);

#endif

#ifndef FS_H
#define FS_H

#include <stddef.h>

#define MAX_NAME      32
#define MAX_PATH      128
#define MAX_CHILDREN  32

enum {
    FS_NODE_FILE = 1,
    FS_NODE_DIR  = 2
};

typedef struct FsNode {
    char name[MAX_NAME];
    char path[MAX_PATH];
    int  type;               // FILE or DIR

    struct FsNode *parent;
    struct FsNode *children[MAX_CHILDREN];
    int  child_count;
} FsNode;

// 初始化
void fs_init(void);

// 基本操作
FsNode *fs_root(void);
FsNode *fs_resolve(const FsNode *cwd, const char *path);
FsNode *fs_mkdir(const FsNode *cwd, const char *path, int recursive);
FsNode *fs_touch(const FsNode *cwd, const char *path);
int     fs_rm(const FsNode *cwd, const char *path);

// 顯示
void fs_list(const FsNode *dir);
void fs_dump(const FsNode *node);

#endif

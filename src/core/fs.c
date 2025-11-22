#include "fs.h"
#include "terminal.h"
#include "string.h"

// -----------------------------------------------------------
// static node pool
// -----------------------------------------------------------
static FsNode node_pool[256];
static int node_pool_used = 0;

static FsNode* alloc_node(void) {
    if (node_pool_used >= 256)
        return NULL;

    FsNode *n = &node_pool[node_pool_used++];
    k_memset(n, 0, sizeof(FsNode));
    return n;
}

// -----------------------------------------------------------
// root
// -----------------------------------------------------------
static FsNode *root = NULL;

FsNode *fs_root(void) {
    if (!root) {
        root = alloc_node();
        root->type = FS_NODE_DIR;
        k_strcpy(root->name, "/");
        k_strcpy(root->path, "/");
        root->parent = NULL;
    }
    return root;
}

void fs_init(void) {
    FsNode *r = fs_root();
}

// -----------------------------------------------------------
// split path "a/b/c" → parent:"a/b", name:"c"
// -----------------------------------------------------------
static void split_path(const char *path, char *parent, char *name) {
    const char *last = k_strrchr(path, '/');

    if (!last) {
        k_strcpy(parent, "");
        k_strcpy(name, path);
        return;
    }

    if (last == path) {
        k_strcpy(parent, "/");
        k_strcpy(name, last + 1);
        return;
    }

    int plen = last - path;
    if (plen >= MAX_PATH) plen = MAX_PATH - 1;

    k_memcpy(parent, path, plen);
    parent[plen] = 0;

    k_strcpy(name, last + 1);
}

// -----------------------------------------------------------
// mkdir
// -----------------------------------------------------------
FsNode *fs_mkdir(const FsNode *cwd, const char *path, int recursive)
{
    char parent[MAX_PATH];
    char name[MAX_NAME];

    split_path(path, parent, name);

    FsNode *base = (parent[0] ? fs_resolve(cwd, parent) : (FsNode*)cwd);
    if (!base || base->type != FS_NODE_DIR) {

        if (!recursive)
            return NULL;

        // recursive: mkdir -p
        if (k_strcmp(parent, "") != 0 && k_strcmp(parent, ".") != 0) {
            FsNode *pnode = fs_mkdir(cwd, parent, 1);
            if (!pnode) return NULL;
            base = pnode;
        } else {
            base = (FsNode*)cwd;
        }
    }

    for (int i = 0; i < base->child_count; i++) {
        FsNode *c = base->children[i];
        if (c && k_strcmp(c->name, name) == 0)
            return c; // already exists
    }

    if (base->child_count >= MAX_CHILDREN)
        return NULL;

    FsNode *child = alloc_node();
    child->type = FS_NODE_DIR;
    child->parent = base;
    k_strcpy(child->name, name);

    if (base == fs_root())
        k_snprintf(child->path, sizeof(child->path), "/%s", name);
    else
        k_snprintf(child->path, sizeof(child->path), "%s/%s", base->path, name);

    base->children[base->child_count++] = child;
    return child;
}

// -----------------------------------------------------------
// touch
// -----------------------------------------------------------
FsNode *fs_touch(const FsNode *cwd, const char *path)
{
    char parent[MAX_PATH];
    char name[MAX_NAME];

    split_path(path, parent, name);

    FsNode *base = (parent[0] ? fs_resolve(cwd, parent) : (FsNode*)cwd);
    if (!base || base->type != FS_NODE_DIR)
        return NULL;

    for (int i = 0; i < base->child_count; i++) {
        FsNode *c = base->children[i];
        if (c && k_strcmp(c->name, name) == 0)
            return c;
    }

    if (base->child_count >= MAX_CHILDREN)
        return NULL;

    FsNode *child = alloc_node();
    child->type = FS_NODE_FILE;
    child->parent = base;
    k_strcpy(child->name, name);

    if (base == fs_root())
        k_snprintf(child->path, sizeof(child->path), "/%s", name);
    else
        k_snprintf(child->path, sizeof(child->path), "%s/%s", base->path, name);

    base->children[base->child_count++] = child;
    return child;
}

// -----------------------------------------------------------
// rm
// -----------------------------------------------------------
int fs_rm(const FsNode *cwd, const char *path)
{
    FsNode *target = fs_resolve(cwd, path);
    if (!target || target == root)
        return -1;

    FsNode *p = target->parent;

    for (int i = 0; i < p->child_count; i++) {
        if (p->children[i] == target) {
            for (int j = i; j < p->child_count - 1; j++)
                p->children[j] = p->children[j + 1];
            p->children[p->child_count - 1] = NULL;
            p->child_count--;
            return 0;
        }
    }
    return -1;
}

// -----------------------------------------------------------
// list
// -----------------------------------------------------------
void fs_list(const FsNode *dir)
{
    if (!dir || dir->type != FS_NODE_DIR)
        return;

    for (int i = 0; i < dir->child_count; i++) {
        FsNode *c = dir->children[i];
        terminal_write(c->name);
        if (c->type == FS_NODE_DIR)
            terminal_write("/");
        terminal_putc('\n');
    }
}

// -----------------------------------------------------------
// dump (file contents placeholder)
// -----------------------------------------------------------
void fs_dump(const FsNode *node)
{
    if (!node) {
        terminal_write_line("cat: invalid node");
        return;
    }
    if (node->type != FS_NODE_FILE) {
        terminal_write_line("cat: not a file");
        return;
    }

    terminal_write_line("(empty file)");
}

// -----------------------------------------------------------
// resolve path
// -----------------------------------------------------------
FsNode *fs_resolve(const FsNode *cwd, const char *path)
{
    if (!path || !*path)
        return (FsNode*)cwd;

    FsNode *cur = (path[0] == '/' ? fs_root() : (FsNode*)cwd);

    char buf[MAX_PATH];
    k_strcpy(buf, path);

    char *token = k_strtok(buf, "/");
    while (token) {

        if (k_strcmp(token, ".") == 0) {
            // nothing
        }
        else if (k_strcmp(token, "..") == 0) {
            if (cur->parent) cur = cur->parent;
        }
        else {
            int found = 0;
            for (int i = 0; i < cur->child_count; i++) {
                FsNode *c = cur->children[i];
                if (c && k_strcmp(c->name, token) == 0) {
                    cur = c;
                    found = 1;
                    break;
                }
            }
            if (!found) return NULL;
        }

        token = k_strtok(NULL, "/");
    }

    return cur;
}
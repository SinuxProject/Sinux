#include "vfs.h"
#include "../../mm/vmm.h"
#include "../../lib/string.h"
#include "../../lib/printk.h"

#define MAX_MOUNTS 32

typedef struct {
    char      path[256];
    fs_ops_t *ops;
    inode_t  *root;
    bool      active;
} mount_t;

static mount_t mounts[MAX_MOUNTS];
static int     mount_count = 0;

void
vfs_init(void) {
    kmemset(mounts, 0, sizeof(mounts));
}

int
vfs_mount_root(const char *path, fs_ops_t *ops, inode_t *root)
{
    if (mount_count >= MAX_MOUNTS) return -ENOSPC;
    mount_t *m = &mounts[mount_count++];
    kstrncpy(m->path, path, sizeof(m->path));
    m->ops    = ops;
    m->root   = root;
    m->active = true;
    printk("[INFO] vfs: mounted %s\n", path);
    return 0;
}

int
vfs_mount(const char *path, fs_ops_t *ops, void *data)
{
    (void)data;
    return vfs_mount_root(path, ops, NULL);
}

static mount_t *
find_mount(const char *path)
{
    mount_t *best     = NULL;
    size_t   best_len = 0;
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mounts[i].active) continue;
        size_t len = kstrlen(mounts[i].path);
        if (kstrncmp(path, mounts[i].path, len) == 0 && len > best_len) {
            best     = &mounts[i];
            best_len = len;
        }
    }
    return best;
}

inode_t *
vfs_lookup(const char *path)
{
    mount_t *m = find_mount(path);
    if (!m || !m->root) return NULL;

    const char *rel = path + kstrlen(m->path);
    if (*rel == '/') rel++;
    if (*rel == '\0') return m->root;

    if (!m->ops->lookup) return NULL;

    char     component[256];
    inode_t *cur = m->root;
    const char *p = rel;

    while (*p && cur) {
        int i = 0;
        while (*p && *p != '/' && i < 255) component[i++] = *p++;
        component[i] = '\0';
        if (*p == '/') p++;
        if (!component[0]) continue;
        if (!cur->ops || !cur->ops->lookup) return NULL;
        cur = cur->ops->lookup(cur, component);
    }
    return cur;
}

file_t *
vfs_open(const char *path, int flags)
{
    inode_t *ino = vfs_lookup(path);
    if (!ino) {
        if (!(flags & O_CREAT)) return NULL;
        vfs_create(path, FT_REG);
        ino = vfs_lookup(path);
        if (!ino) return NULL;
    }

    file_t *f = kmalloc(sizeof(file_t));
    if (!f) return NULL;
    f->inode    = ino;
    f->pos      = 0;
    f->flags    = flags;
    f->refcount = 1;
    f->ops      = ino->ops;

    if (f->ops && f->ops->open) f->ops->open(ino, f);
    if (flags & O_TRUNC)  ino->size = 0;
    if (flags & O_APPEND) f->pos = ino->size;
    return f;
}

int
vfs_close(file_t *f)
{
    if (!f) return -EBADF;
    if (--f->refcount > 0) return 0;
    if (f->ops && f->ops->close) f->ops->close(f);
    kfree(f);
    return 0;
}

int64_t vfs_read (file_t *f, void *buf, size_t n) {
    if (!f || !f->ops || !f->ops->read)  return -EBADF;
    return f->ops->read(f, buf, n);
}
int64_t vfs_write(file_t *f, const void *buf, size_t n) {
    if (!f || !f->ops || !f->ops->write) return -EBADF;
    return f->ops->write(f, buf, n);
}
int64_t vfs_readdir(file_t *f, dentry_t *d) {
    if (!f || !f->ops || !f->ops->readdir) return 0;
    return f->ops->readdir(f, d);
}

static void
split_path(const char *path, char *dir, char *name)
{
    kstrncpy(dir, path, 255);
    char *last = dir;
    for (char *p = dir; *p; p++) if (*p == '/') last = p;
    if (last == dir) {
        /* Top-level path ("/foo"): the name must live in its own
         * buffer — aliasing dir+1 breaks because kstrcpy(dir, "/")
         * overwrites dir[1] with NUL (all top-level creates
         * previously failed with -EINVAL). */
        kstrcpy(name, dir + 1);
        kstrcpy(dir, "/");
    } else {
        kstrcpy(name, last + 1);
        *last = '\0';
    }
}

int
vfs_create(const char *path, file_type_t type)
{
    char dir[256]; char name[256];
    split_path(path, dir, name);
    if (!name[0]) return -EINVAL;
    inode_t *parent = vfs_lookup(dir);
    if (!parent || parent->type != FT_DIR) return -ENOENT;
    if (!parent->ops || !parent->ops->create) return -EACCES;
    return parent->ops->create(parent, name, type);
}

int
vfs_unlink(const char *path)
{
    char dir[256]; char name[256];
    split_path(path, dir, name);
    if (!name[0]) return -EINVAL;
    inode_t *parent = vfs_lookup(dir);
    if (!parent) return -ENOENT;
    if (!parent->ops || !parent->ops->unlink) return -EACCES;
    return parent->ops->unlink(parent, name);
}

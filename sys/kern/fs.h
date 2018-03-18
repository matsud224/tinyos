#pragma once
#include <kern/kernlib.h>
#include <kern/lock.h>

#define MAX_FILE_NAME 255

#define VNO_INVALID 0

#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000


enum vnode_flags {
  V_DIRTY = 0x1,
};

struct vnode {
  struct fs *fs;
  const struct vnode_ops *ops;
  const struct file_ops *file_ops;
  devno_t devno; //for block/charcter device
  int ref;
  u32 flags;
  vno_t number;
  mutex mtx;
  struct list_head fs_link;
};

struct stat {
  u32 st_mode;
  devno_t st_dev;
  size_t st_size;
};

struct vnode_ops {
  struct vnode *(*lookup)(struct vnode *vno, const char *name);
  int (*mknod)(struct vnode *parent, const char *name, int mode, devno_t devno);
  int (*link)(struct vnode *parent, const char *name, struct vnode *vno);
  int (*unlink)(struct vnode *parent, const char *name, struct vnode *vno);
  int (*stat)(struct vnode *vno, struct stat *buf);
  void (*vfree)(struct vnode *vno);
  void (*vsync)(struct vnode *vno);
};

struct fs_ops {
  struct vnode *(*getroot)(struct fs *fs);
};

struct fs {
  const struct fs_ops *fs_ops;
  struct list_head vnode_list;
};

struct fstype_ops {
  struct fs *(*mount)(devno_t devno);
};

void fstype_register(const char *name, const struct fstype_ops *ops);

void fs_init(void);
int fs_mountroot(const char *name, devno_t devno);

struct file *open(const char *path, int flags);
int mknod(const char *path, int mode, devno_t dev);
int link(const char *oldpath, const char *newpath);
int unlink(const char *path);
int stat(const char *path, struct stat *buf);

void vnode_init(struct vnode *vno, vno_t number, struct fs *fs, const struct vnode_ops *ops, const struct file_ops *file_ops);
void vnode_lock(struct vnode *vno);
void vnode_unlock(struct vnode *vno);
void vnode_hold(struct vnode *vno);
void vnode_release(struct vnode *vno);
void vnode_markdirty(struct vnode *vno);
struct vnode *vcache_find(struct fs *fs, vno_t number);
int vcache_add(struct fs *fs, struct vnode *vno);
void vcache_remove(struct vnode *vno);


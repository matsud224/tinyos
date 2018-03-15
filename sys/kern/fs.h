#pragma once
#include <kern/kernlib.h>

#define MAX_FILE_NAME 255

#define VNO_INVALID 0

enum vtype {
  V_REGULAR, V_BLKDEV, V_CHARDEV, V_PIPE,
};

struct vnode {
  struct fs *fs;
  const struct vnode_ops *ops;
  int type;
  devno_t devno;
  int ref;
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
  struct vnode *(*create)(struct vnode *vno, const char *name);
  struct vnode *(*lookup)(struct vnode *vno, const char *name);
  int (*mknod)(struct vnode *vno, int mode, devno_t devno);
  int (*link)(struct vnode *vno, const char *name);
  int (*unlink)(struct vnode *vno, const char *name);
  int (*stat)(struct vnode *vno, struct stat *buf);
  void (*vfree)(struct vnode *vno);
};

struct fs_ops {
  struct vnode *(*getroot)(struct fs *fs);
};

struct fs {
  const struct fs_ops *fs_ops;
  const struct file_ops *file_ops;
  struct list_head vnode_list;
};

struct fstype_ops {
  struct fs *(*mount)(devno_t devno);
};

void fstype_register(const char *name, const struct fstype_ops *ops);

struct file *open(const char *path);
int mknod(const char *path, int mode, devno_t dev);
int link(const char *oldpath, const char *newpath);
int unlink(const char *path);
int stat(const char *path, struct stat *buf);


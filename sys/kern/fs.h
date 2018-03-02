#pragma once
#include <kern/kernlib.h>

struct vnode {
  struct fs *fs;
  const struct vnode_ops *ops;
  u32 number;
  u16 mode;
  u16 nlinks;
  u32 size;
  u32 atime;
  u32 mtime;
  u32 ctime;
  struct file_buf 
};

#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK	 0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)

struct vnode_ops {
  struct vnode *(*create)(struct vnode *vno, const char *name);
  struct vnode *(*lookup)(struct vnode *vno, const char *name);
  int (*mknod)(struct vnode *vno, const char *name);
  int (*link)(struct vnode *vno, const char *name);
  int (*unlink)(struct vnode *vno, const char *name);
  int (*stat)(struct vnode *vno, struct stat *buf);
};

struct fs_ops {
  struct inode *(*getroot)(struct fs *fs);
};

struct fs {
  const struct fs_ops *fs_ops;
  const struct file_ops *file_ops;
};

struct fstype_ops {
  struct fs *(*mount)(void *source);
};

struct fstype {
  const char *name;
  const struct fstype_ops *ops;
};


void fsinfo_add(const struct fsinfo *info);
int fs_mountroot(const char *name, void *source);
struct inode *fs_nametoi(const char *path);
int fs_read(struct inode *inode, u8 *base, u32 offset, size_t count);

struct file *open(const char *path);
int mknod(const char *path, int mode, dev_t dev);
int link(const char *oldpath, const char *newpath);
int unlink(const char *path);
int stat(const char *path);


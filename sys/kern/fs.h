#pragma once
#include <kern/kernlib.h>
#include <kern/addrspace.h>
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
  //struct addrspace addrspace;
  struct list_head fs_link;
};

struct stat {
  unsigned short st_dev;
  unsigned short st_ino;
  unsigned short st_mode;
  unsigned short st_nlink;
  unsigned short st_uid;
  unsigned short st_gid;
  int   st_rdev;
  long  st_size;
  long  st_atime;
  long  st_spare1;
  long  st_mtime;
  long  st_spare2;
  long  st_ctime;
  long  st_spare3;
  long  st_blksize;
  long  st_blocks;
  long	st_spare4[2];
};

struct dirent {
  vno_t d_vno;
  char d_name[MAX_FILENAME_LEN+1];
};

enum lookup_result {
  LOOKUP_FOUND = 0,
  LOOKUP_NOTFOUND,
  LOOKUP_ERROR,
};

struct vnode_ops {
  int (*lookup)(struct vnode *vno, const char *name, struct vnode **found);
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

struct vnode *name_to_vnode(const char *path, struct vnode **parent, char **fname);

int sys_open(const char *path, int flags);
int sys_mknod(const char *path, int mode, devno_t devno);
int sys_link(const char *oldpath, const char *newpath);
int sys_unlink(const char *path);
int sys_stat(const char *path, struct stat *buf);
int sys_fstat(int fd, struct stat *buf);

void vnode_init(struct vnode *vno, vno_t number, struct fs *fs, const struct vnode_ops *ops, const struct file_ops *file_ops, devno_t devno);
void vnodes_lock(void);
void vnodes_unlock(void);
void vnode_hold(struct vnode *vno);
void vnode_release(struct vnode *vno);
void vnode_markdirty(struct vnode *vno);
struct vnode *vcache_find(struct fs *fs, vno_t number);
int vcache_add(struct fs *fs, struct vnode *vno);
void vcache_remove(struct vnode *vno);
void vsync(void);
